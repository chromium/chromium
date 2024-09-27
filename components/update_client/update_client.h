// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
#define COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/version.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/update_client_errors.h"

// The UpdateClient class is a facade with a simple interface. The interface
// exposes a few APIs to install a CRX or update a group of CRXs.
//
// The difference between a CRX install and a CRX update is relatively minor.
// The terminology going forward will use the word "update" to cover both
// install and update scenarios, except where details regarding the install
// case are relevant.
//
// Handling an update consists of a series of actions such as sending an update
// check to the server, followed by parsing the server response, identifying
// the CRXs that require an update, downloading the differential update if
// it is available, unpacking and patching the differential update, then
// falling back to trying a similar set of actions using the full update.
// At the end of this process, completion pings are sent to the server,
// as needed, for the CRXs which had updates.
//
// As a general idea, this code handles the action steps needed to update
// a group of components serially, one step at a time. However, concurrent
// execution of calls to UpdateClient::Update is possible, therefore,
// queuing of updates could happen in some cases. More below.
//
// The UpdateClient class features a subject-observer interface to observe
// the CRX state changes during an update.
//
// Most of the code in the public interface runs on a SequencedTaskRunner. This
// task runner corresponds to the browser UI thread but it can be any other
// sequenced task runner. There are parts of the installer interface that run
// on blocking task runners, which are usually sequences managed by the thread
// pool.
//
// Using the UpdateClient requires creating an instance, adding observers, and
// providing an installer instance, as shown below:
//
//    std::unique_ptr<UpdateClient> update_client(UpdateClientFactory(...));
//    update_client->AddObserver(&observer);
//    std::vector<std::string> ids;
//    ids.push_back(...));
//    update_client->Update(ids, base::BindOnce(...), base::BindOnce(...));
//
// UpdateClient::Update takes two callbacks as parameters. First callback
// allows the client of this code to provide an instance of CrxComponent
// data structure that specifies additional parameters of the update.
// CrxComponent has a CrxInstaller data member, which must be provided by the
// callers of this class. The second callback indicates that this non-blocking
// call has completed.
//
// There could be several ways of triggering updates for a CRX, user-initiated,
// or timer-based. Since the execution of updates is concurrent, the parameters
// for the update must be provided right before the update is handled.
// Otherwise, the version of the CRX set in the CrxComponent may not be correct.
//
// The UpdateClient public interface includes two functions: Install and
// Update. These functions correspond to installing one CRX immediately as a
// foreground activity (Install), and updating a group of CRXs silently in the
// background (Update). This distinction is important. Background updates are
// queued up and their actions run serially, one at a time, for the purpose of
// conserving local resources such as CPU, network, and I/O.
// On the other hand, installs are never queued up but run concurrently, as
// requested by the user.
//
// The update client introduces a runtime constraint regarding interleaving
// updates and installs. If installs or updates for a given CRX are in progress,
// then installs for the same CRX will fail with a specific error.
//
// Implementation details.
//
// The implementation details below are not relevant to callers of this
// code. However, these design notes are relevant to the owners and maintainers
// of this module.
//
// The design for the update client consists of a number of abstractions
// such as: task, update engine, update context, and action.
// The execution model for these abstractions is simple. They usually expose
// a public, non-blocking Run function, and they invoke a callback when
// the Run function has completed.
//
// A task is the unit of work for the UpdateClient. A task is associated
// with a single call of the Update function. A task represents a group
// of CRXs that are updated together.
//
// The UpdateClient is responsible for the queuing of tasks, if queuing is
// needed.
//
// When the task runs, it calls the update engine to handle the updates for
// the CRXs associated with the task. The UpdateEngine is the abstraction
// responsible for breaking down the update in a set of discrete steps, which
// are implemented as actions, and running the actions.
//
// The UpdateEngine maintains a set of UpdateContext instances. Each of
// these instances maintains the update state for all the CRXs belonging to
// a given task. The UpdateContext contains a queue of CRX ids.
// The UpdateEngine will handle updates for the CRXs in the order they appear
// in the queue, until the queue is empty.
//
// The update state for each CRX is maintained in a container of CrxUpdateItem*.
// As actions run, each action updates the CRX state, represented by one of
// these CrxUpdateItem* instances.
//
// Although the UpdateEngine can and will run update tasks concurrently, the
// actions of a task are run sequentially.
//
// The Action is a polymorphic type. There is some code reuse for convenience,
// implemented as a mixin. The polymorphic behavior of some of the actions
// is achieved using a template method.
//
// State changes of a CRX could generate events, which are observed using a
// subject-observer interface.
//
// The actions chain up. In some sense, the actions implement a state machine,
// as the CRX undergoes a series of state transitions in the process of
// being checked for updates and applying the update.

class PrefRegistrySimple;

namespace base {
class FilePath;
}

namespace crx_file {
enum class VerifierFormat;
}

namespace update_client {

class Configurator;
enum class Error;
struct CrxUpdateItem;

enum class ComponentState {
  kNew,
  kChecking,
  kCanUpdate,
  kDownloadingDiff,
  kDownloading,
  kUpdatingDiff,
  kUpdating,
  kUpdated,
  kUpToDate,
  kUpdateError,
  kRun,
  kLastStatus
};

// Defines an interface for a generic CRX installer.
class CrxInstaller : public base::RefCountedThreadSafe<CrxInstaller> {
 public:
  // Contains the result of the Install operation.
  struct Result {
    Result() = default;
    explicit Result(int error, int extended_error = 0)
        : result({.category_ = error == 0 ? ErrorCategory::kNone
                                          : ErrorCategory::kInstall,
                  .code_ = error,
                  .extra_ = extended_error}) {}
    explicit Result(InstallError error, int extended_error = 0)
        : result({.category_ = error == InstallError::NONE
                                   ? ErrorCategory::kNone
                                   : ErrorCategory::kInstall,
                  .code_ = static_cast<int>(error),
                  .extra_ = extended_error}) {}
    explicit Result(CategorizedError error) : result(error) {}

    // The install is successful if and only if result.category_ is kNone.
    // result.code_ may be non-zero for a successful install.
    CategorizedError result;

    // Localized text displayed to the user, if applicable.
    std::string installer_text;

    // Shell command run at the end of the install, if applicable. This string
    // must be escaped to be a command line.
    std::string installer_cmd_line;
  };

  struct InstallParams {
    InstallParams(const std::string& run,
                  const std::string& arguments,
                  const std::string& server_install_data);
    std::string run;
    std::string arguments;
    std::string server_install_data;
  };

  using ProgressCallback = base::RepeatingCallback<void(int progress)>;
  using Callback = base::OnceCallback<void(const Result& result)>;

  // Called on the main sequence when there was a problem unpacking or
  // verifying the CRX. |error| is a non-zero value which is only meaningful
  // to the caller.
  virtual void OnUpdateError(int error) = 0;

  // Called by the update service when a CRX has been unpacked
  // and it is ready to be installed. This method may be called from a
  // sequence other than the main sequence.
  // |unpack_path| contains the temporary directory with all the unpacked CRX
  // files.
  // |pubkey| contains the public key of the CRX in the PEM format, without the
  // header and the footer.
  // |install_params| is an optional parameter which provides the name and the
  // arguments for a binary program which is invoked as part of the install or
  // update flows.
  // |progress_callback| reports installer progress. This callback must be run
  // directly instead of posting it.
  // |callback| must be the last callback invoked and it indicates that the
  // install flow has completed.
  virtual void Install(const base::FilePath& unpack_path,
                       const std::string& public_key,
                       std::unique_ptr<InstallParams> install_params,
                       ProgressCallback progress_callback,
                       Callback callback) = 0;

  // Returns the path to the installed `file`. If there is no such path (for
  // example because no version of the item is installed), returns nullopt.
  // Called on the main sequence, can't block.
  virtual std::optional<base::FilePath> GetInstalledFile(
      const std::string& file) = 0;

  // Called when a CRX has been unregistered and all versions should
  // be uninstalled from disk. Returns true if uninstallation is supported,
  // and false otherwise.
  virtual bool Uninstall() = 0;

 protected:
  friend class base::RefCountedThreadSafe<CrxInstaller>;

  virtual ~CrxInstaller() = default;
};

// Defines an interface to handle |action| elements in the update response.
// The current implementation only handles run actions bound to a CRX, meaning
// that such CRX is unpacked and an executable file, contained inside the CRX,
// is run, then the results of the invocation are collected by the callback.
class ActionHandler : public base::RefCountedThreadSafe<ActionHandler> {
 public:
  using Callback =
      base::OnceCallback<void(bool succeeded, int error_code, int extra_code1)>;

  virtual void Handle(const base::FilePath& action,
                      const std::string& session_id,
                      Callback callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<ActionHandler>;

  virtual ~ActionHandler() = default;
};

// A dictionary of installer-specific, arbitrary name-value pairs, which
// may be used in the update checks requests.
using InstallerAttributes = std::map<std::string, std::string>;

struct CrxComponent {
  CrxComponent();
  CrxComponent(const CrxComponent& other);
  CrxComponent& operator=(const CrxComponent& other);
  ~CrxComponent();

  // Optional SHA256 hash of the CRX's public key. If not supplied, the
  // unpacker can accept any CRX for this app, provided that the CRX meets the
  // VerifierFormat requirements specified by the service's configurator.
  // Callers that know or need a specific developer signature on acceptable CRX
  // files must provide this.
  std::vector<uint8_t> pk_hash;

  scoped_refptr<CrxInstaller> installer;
  scoped_refptr<ActionHandler> action_handler;

  std::string app_id;

  // The current version if the CRX is updated. Otherwise, "0" or "0.0" if
  // the CRX is installed.
  base::Version version;

  // Optional. This additional parameter ("ap") is sent to the server, which
  // often uses it to distinguish between variants of the software that were
  // chosen at install time.
  std::string ap;

  // If nonempty, the brand is an uppercase 4-letter string that describes the
  // flavor, branding, or provenance of the software.
  std::string brand;

  // If populated, the `install_data_index` is sent to the update server as part
  // of the `data` element. The server will provide corresponding installer data
  // in the update response. This data is then provided to the installer when
  // running it.
  std::string install_data_index;

  std::string fingerprint;  // Optional.
  std::string name;         // Optional.

  // Optional.
  // Valid values for the name part of an attribute match
  // ^[-_a-zA-Z0-9]{1,256}$ and valid values the value part of an attribute
  // match ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
  InstallerAttributes installer_attributes;

  // Specifies that the update checks and pings associated with this component
  // require confidentiality. The default for this value is |true|. As a side
  // note, the confidentiality of the downloads is enforced by the server,
  // which only returns secure download URLs in this case.
  bool requires_network_encryption = true;

  // Specifies the strength of package validation required for the item.
  crx_file::VerifierFormat crx_format_requirement =
      crx_file::VerifierFormat::CRX3_WITH_PUBLISHER_PROOF;

  // True if and only if this item may be updated.
  bool updates_enabled = true;

  // Reasons why this component/extension is disabled.
  std::vector<int> disabled_reasons;

  // Information about where the component/extension was installed from.
  // For extension, this information is set from the update service, which
  // gets the install source from the update URL.
  std::string install_source;

  // Information about where the component/extension was loaded from.
  // For extensions, this information is inferred from the extension
  // registry.
  std::string install_location;

  // Information about the channel to send to the update server when updating
  // the component. This optional field is typically populated by policy and is
  // only populated on managed devices.
  std::string channel;

  // A version prefix sent to the server in the case of version pinning. The
  // server should not respond with an update to a version that does not match
  // this prefix. If no prefix is specified, the client will accept any version.
  std::string target_version_prefix;

  // An indicator sent to the server to advise whether it may perform a version
  // downgrade of this item.
  bool rollback_allowed = false;

  // An indicator sent to the server to advise whether it may perform an
  // over-install on this item.
  bool same_version_update_allowed = false;

  // Specifies that this CRX can be cached for differential updates.
  // The default for this value is |true|.
  bool allow_cached_copies = true;

  // Specifies whether updates can be initiated on metered network connections.
  bool allow_updates_on_metered_connection = true;
};

// Called when a non-blocking call of UpdateClient completes.
using Callback = base::OnceCallback<void(Error error)>;

// All methods are safe to call only from the browser's main thread. Once an
// instance of this class is created, the reference to it must be released
// only after the thread pools of the browser process have been destroyed and
// the browser process has gone single-threaded.
class UpdateClient : public base::RefCountedThreadSafe<UpdateClient> {
 public:
  // Calls `callback` with `CrxComponent` instances corresponding to the
  // component ids passed as an argument. The order of components in the input
  // and output vectors must match. If the instance of the `CrxComponent` is
  // not available for some reason, implementors of the callback must not skip
  // the component, and instead, they must insert a `nullopt` value in the
  // output vector.
  using CrxDataCallback = base::OnceCallback<void(
      const std::vector<std::string>& ids,
      base::OnceCallback<void(const std::vector<std::optional<CrxComponent>>&)>
          callback)>;

  // Called when state changes occur during an Install or Update call.
  using CrxStateChangeCallback =
      base::RepeatingCallback<void(const CrxUpdateItem& item)>;

  // Defines an interface to observe the UpdateClient. It provides
  // notifications when state changes occur for the service itself or for the
  // registered CRXs.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called by the update client when a component makes progress. This could
    // be a state change or progress within a state, such as additional
    // downloaded bytes or installer progress.
    virtual void OnEvent(const CrxUpdateItem& item) = 0;
  };

  // Packs the parameters for sending a ping.
  struct PingParams {
    int event_type = 0;
    int result = 0;
    ErrorCategory error_category = ErrorCategory::kNone;
    int error_code = 0;
    int extra_code1 = 0;
    std::string app_command_id;
  };

  // Adds an observer for this class. An observer should not be added more
  // than once. The caller retains the ownership of the observer object.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer. It is safe for an observer to be removed while
  // the observers are being notified.
  virtual void RemoveObserver(Observer* observer) = 0;

  // Installs the specified CRX. Calls back on |callback| after the
  // update has been handled. Provides state change notifications through
  // invocations of the optional |crx_state_change_callback| callback.
  // The |error| parameter of the |callback| contains an error code in the case
  // of a run-time error, or 0 if the install has been handled successfully.
  // Overlapping calls of this function are executed concurrently, as long as
  // the id parameter is different, meaning that installs of different
  // components are parallelized.
  // The |Install| function is intended to be used for foreground installs of
  // one CRX. These cases are usually associated with on-demand install
  // scenarios, which are triggered by user actions. Installs are never
  // queued up.
  // Returns a closure that can be called to cancel the installation.
  virtual base::RepeatingClosure Install(
      const std::string& id,
      CrxDataCallback crx_data_callback,
      CrxStateChangeCallback crx_state_change_callback,
      Callback callback) = 0;

  // Does an update check with the server, gets an update response, but it does
  // not continue further with downloading, nor installing the payload.
  virtual void CheckForUpdate(const std::string& id,
                              CrxDataCallback crx_data_callback,
                              CrxStateChangeCallback crx_state_change_callback,
                              bool is_foreground,
                              Callback callback) = 0;

  // Updates the specified CRXs. Calls back on |crx_data_callback| before the
  // update is attempted to give the caller the opportunity to provide the
  // instances of CrxComponent to be used for this update. Provides state change
  // notifications through invocations of the optional
  // |crx_state_change_callback| callback.
  // The |Update| function is intended to be used for background updates of
  // several CRXs. Overlapping calls to this function result in a queuing
  // behavior, and the execution of each call is serialized. In addition,
  // updates are always queued up when installs are running. The |is_foreground|
  // parameter must be set to true if the invocation of this function is a
  // result of a user initiated update.
  virtual void Update(const std::vector<std::string>& ids,
                      CrxDataCallback crx_data_callback,
                      CrxStateChangeCallback crx_state_change_callback,
                      bool is_foreground,
                      Callback callback) = 0;

  // Sends a ping for `crx_component`. The current implementation of this
  // function only sends a best-effort ping. It has no other side effects
  // regarding installs or updates done through an instance of this class.
  virtual void SendPing(const CrxComponent& crx_component,
                        PingParams ping_params,
                        Callback callback) = 0;

  // Returns status details about a CRX update. The function returns true in
  // case of success and false in case of errors, such as |id| was
  // invalid or not known.
  virtual bool GetCrxUpdateState(const std::string& id,
                                 CrxUpdateItem* update_item) const = 0;

  // Returns true if the |id| is found in any running task.
  virtual bool IsUpdating(const std::string& id) const = 0;

  // Cancels the queued updates and makes a best effort to stop updates in
  // progress as soon as possible. Some updates may not be stopped, in which
  // case, the updates will run to completion. Calling this function has no
  // effect if updates are not currently executed or queued up.
  virtual void Stop() = 0;

 protected:
  friend class base::RefCountedThreadSafe<UpdateClient>;

  virtual ~UpdateClient() = default;
};

// Creates an instance of the update client.
scoped_refptr<UpdateClient> UpdateClientFactory(
    scoped_refptr<Configurator> config);

// This must be called prior to the construction of any Configurator that
// contains a PrefService.
void RegisterPrefs(PrefRegistrySimple* registry);

// This must be called prior to the construction of any Configurator that
// needs access to local user profiles.
// This function is mostly used for ExtensionUpdater, which requires update
// info from user profiles.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UPDATE_CLIENT_H_
