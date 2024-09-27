// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client.h"
#include "url/gurl.h"

namespace update_client {

class Configurator;
struct CrxUpdateItem;
struct UpdateContext;

// Describes a CRX component managed by the UpdateEngine. Each instance of
// this class is associated with one instance of UpdateContext.
class Component {
 public:
  using CallbackHandleComplete = base::OnceCallback<void()>;

  Component(const UpdateContext& update_context, const std::string& id);
  Component(const Component&) = delete;
  Component& operator=(const Component&) = delete;
  ~Component();

  // Handles the current state of the component and makes it transition
  // to the next component state before |callback_handle_complete_| is invoked.
  void Handle(CallbackHandleComplete callback_handle_complete);

  CrxUpdateItem GetCrxUpdateItem() const;

  // Called by the UpdateEngine when an update check for this component is done.
  void SetUpdateCheckResult(const std::optional<ProtocolParser::Result>& result,
                            ErrorCategory error_category,
                            int error);

  // Called by the UpdateEngine when a component enters a wait for throttling
  // purposes.
  void NotifyWait();

  // Returns true if the component has reached a final state and no further
  // handling and state transitions are possible.
  bool IsHandled() const { return is_handled_; }

  // Returns true if an update is available for this component, meaning that
  // the update server has return a response containing an update.
  bool IsUpdateAvailable() const { return is_update_available_; }

  void Cancel() { state_->Cancel(); }

  base::TimeDelta GetUpdateDuration() const;

  ComponentState state() const { return state_->state(); }

  std::string id() const { return id_; }

  const std::optional<CrxComponent>& crx_component() const {
    return crx_component_;
  }
  void set_crx_component(const CrxComponent& crx_component) {
    crx_component_ = crx_component;
  }

  const base::Version& previous_version() const { return previous_version_; }
  void set_previous_version(const base::Version& previous_version) {
    previous_version_ = previous_version;
  }

  const base::Version& next_version() const { return next_version_; }

  std::string previous_fp() const { return previous_fp_; }
  void set_previous_fp(const std::string& previous_fp) {
    previous_fp_ = previous_fp;
  }

  std::string next_fp() const { return next_fp_; }
  void set_next_fp(const std::string& next_fp) { next_fp_ = next_fp; }

  bool is_foreground() const;

  const std::vector<GURL>& crx_diffurls() const { return crx_diffurls_; }

  bool diff_update_failed() const { return diff_error_code_; }

  ErrorCategory error_category() const { return error_category_; }
  int error_code() const { return error_code_; }
  int extra_code1() const { return extra_code1_; }
  ErrorCategory diff_error_category() const { return diff_error_category_; }
  int diff_error_code() const { return diff_error_code_; }
  int diff_extra_code1() const { return diff_extra_code1_; }

  std::string action_run() const { return action_run_; }

  scoped_refptr<Configurator> config() const;

  std::string session_id() const;

  const std::vector<base::Value::Dict>& events() const { return events_; }

  // Returns a clone of the component events.
  std::vector<base::Value::Dict> GetEvents() const;

 private:
  friend class MockPingManagerImpl;
  friend class UpdateCheckerTest;

  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, SendPing);
  FRIEND_TEST_ALL_PREFIXES(PingManagerTest, RequiresEncryption);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, NoUpdateActionRun);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, UpdateCheckCupError);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, UpdateCheckError);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, UpdateCheckInvalidAp);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest,
                           UpdateCheckRequiresEncryptionError);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, UpdateCheckSuccess);
  FRIEND_TEST_ALL_PREFIXES(UpdateCheckerTest, UpdateCheckUpdateDisabled);

  // Describes an abstraction for implementing the behavior of a component and
  // the transition from one state to another.
  class State {
   public:
    using CallbackNextState =
        base::OnceCallback<void(std::unique_ptr<State> next_state)>;

    State(Component* component, ComponentState state);
    virtual ~State();

    // Handles the current state and initiates a transition to a new state.
    // The transition to the new state is non-blocking and it is completed
    // by the outer component, after the current state is fully handled.
    void Handle(CallbackNextState callback);

    ComponentState state() const { return state_; }

    void Cancel() {
      if (cancel_callback_) {
        std::move(cancel_callback_).Run();
      }
    }

   protected:
    // Initiates the transition to the new state.
    void TransitionState(std::unique_ptr<State> new_state);

    // Makes the current state a final state where no other state transition
    // can further occur.
    void EndState();

    Component& component() { return *component_; }
    const Component& component() const { return *component_; }

    SEQUENCE_CHECKER(sequence_checker_);

    const ComponentState state_;
    base::OnceClosure cancel_callback_;

   private:
    virtual void DoHandle() = 0;

    const raw_ref<Component> component_;
    CallbackNextState callback_next_state_;
  };

  class StateNew : public State {
   public:
    explicit StateNew(Component* component);
    StateNew(const StateNew&) = delete;
    StateNew& operator=(const StateNew&) = delete;
    ~StateNew() override;

   private:
    // State overrides.
    void DoHandle() override;
  };

  class StateChecking : public State {
   public:
    explicit StateChecking(Component* component);
    StateChecking(const StateChecking&) = delete;
    StateChecking& operator=(const StateChecking&) = delete;
    ~StateChecking() override;

   private:
    // State overrides.
    void DoHandle() override;
  };

  class StateUpdateError : public State {
   public:
    explicit StateUpdateError(Component* component);
    StateUpdateError(const StateUpdateError&) = delete;
    StateUpdateError& operator=(const StateUpdateError&) = delete;
    ~StateUpdateError() override;

   private:
    // State overrides.
    void DoHandle() override;
  };

  class StateCanUpdate : public State {
   public:
    explicit StateCanUpdate(Component* component);
    StateCanUpdate(const StateCanUpdate&) = delete;
    StateCanUpdate& operator=(const StateCanUpdate&) = delete;
    ~StateCanUpdate() override;

   private:
    // State overrides.
    void DoHandle() override;
    bool CanTryDiffUpdate() const;
    void GetNextCrxFromCacheComplete(const CrxCache::Result& result);
    void CheckIfCacheContainsPreviousCrxComplete(bool crx_is_in_cache);
  };

  class StateUpToDate : public State {
   public:
    explicit StateUpToDate(Component* component);
    StateUpToDate(const StateUpToDate&) = delete;
    StateUpToDate& operator=(const StateUpToDate&) = delete;
    ~StateUpToDate() override;

   private:
    // State overrides.
    void DoHandle() override;
  };

  class StateDownloading : public State {
   public:
    StateDownloading(Component* component, bool diff);
    StateDownloading(const StateDownloading&) = delete;
    StateDownloading& operator=(const StateDownloading&) = delete;
    ~StateDownloading() override;

   private:
    // State overrides.
    void DoHandle() override;

    void DownloadComplete(
        const base::expected<base::FilePath, CategorizedError>&
            download_result);

    bool diff_;
  };

  class StateUpdatingDiff : public State {
   public:
    explicit StateUpdatingDiff(Component* component);
    StateUpdatingDiff(const StateUpdatingDiff&) = delete;
    StateUpdatingDiff& operator=(const StateUpdatingDiff&) = delete;
    ~StateUpdatingDiff() override;

   private:
    // State overrides.
    void DoHandle() override;

    void PatchingComplete(
        const base::expected<base::FilePath, CategorizedError>&);
    void InstallProgress(int install_progress);
    void InstallComplete(const CrxInstaller::Result& installer_result);
  };

  class StateUpdating : public State {
   public:
    explicit StateUpdating(Component* component);
    StateUpdating(const StateUpdating&) = delete;
    StateUpdating& operator=(const StateUpdating&) = delete;
    ~StateUpdating() override;

   private:
    // State overrides.
    void DoHandle() override;

    void InstallProgress(int install_progress);
    void InstallComplete(const CrxInstaller::Result& installer_result);
  };

  class StateUpdated : public State {
   public:
    explicit StateUpdated(Component* component);
    StateUpdated(const StateUpdated&) = delete;
    StateUpdated& operator=(const StateUpdated&) = delete;
    ~StateUpdated() override;

   private:
    // State overrides.
    void DoHandle() override;
  };

  class StateRun : public State {
   public:
    explicit StateRun(Component* component);
    StateRun(const StateRun&) = delete;
    StateRun& operator=(const StateRun&) = delete;
    ~StateRun() override;

   private:
    // State overrides.
    void DoHandle() override;

    void ActionRunComplete(bool succeeded, int error_code, int extra_code1);
  };

  // Returns true is the update payload for this component can be downloaded
  // by a downloader which can do bandwidth throttling on the client side.
  // The decision may be predicated on the expected size of the download.
  bool CanDoBackgroundDownload(int64_t size) const;

  // Returns true if the component has a differential update.
  bool HasDiffUpdate() const;

  void AppendEvent(base::Value::Dict event);

  // Changes the component state and notifies the caller of the |Handle|
  // function that the handling of this component state is complete.
  void ChangeState(std::unique_ptr<State> next_state);

  // Notifies registered observers about changes in the state of the component.
  // If an UpdateClient::CrxStateChangeCallback is provided as an argument to
  // UpdateClient::Install or UpdateClient::Update function calls, then the
  // callback is invoked as well.
  void NotifyObservers() const;

  void SetParseResult(const ProtocolParser::Result& result);

  // These functions return a specific event. Each data member of the event is
  // represented as a key-value pair in a dictionary value.
  base::Value::Dict MakeEventUpdateComplete() const;
  base::Value::Dict MakeEventDownloadMetrics(
      const CrxDownloader::DownloadMetrics& download_metrics) const;
  base::Value::Dict MakeEventActionRun(bool succeeded,
                                       int error_code,
                                       int extra_code1) const;

  std::unique_ptr<CrxInstaller::InstallParams> install_params() const;

  SEQUENCE_CHECKER(sequence_checker_);

  const std::string id_;
  std::optional<CrxComponent> crx_component_;

  // The status of the updatecheck response.
  std::string status_;

  // Time when an update check for this CRX has happened.
  base::TimeTicks last_check_;

  // Time when the update of this CRX has begun.
  base::TimeTicks update_begin_;

  // A component can be made available for download from several urls.
  std::vector<GURL> crx_urls_;
  std::vector<GURL> crx_diffurls_;

  // The cryptographic hash values for the component payload.
  std::string hash_sha256_;
  std::string hashdiff_sha256_;

  // The expected size of the download as reported by the update server.
  int64_t size_ = -1;
  int64_t sizediff_ = -1;

  // The from/to version and fingerprint values.
  base::Version previous_version_;
  base::Version next_version_;
  std::string previous_fp_;
  std::string next_fp_;

  // Contains the file name of the payload to run. This member is set by
  // the update response parser, when the update response includes a run action.
  std::string action_run_;

  // True if the update check response for this component includes an update.
  bool is_update_available_ = false;

  // The error reported by the update checker.
  int update_check_error_ = 0;

  base::FilePath payload_path_;

  // The byte counts below are valid for the current url being fetched.
  // |total_bytes| is equal to the size of the CRX file and |downloaded_bytes|
  // represents how much has been downloaded up to that point. A value of -1
  // means that the byte count is unknown.
  int64_t downloaded_bytes_ = -1;
  int64_t total_bytes_ = -1;

  // Install progress, in the range of [0, 100]. A value of -1 means that the
  // progress is unknown.
  int install_progress_ = -1;

  // The error information for full and differential updates.
  // The |error_category| contains a hint about which module in the component
  // updater generated the error. The |error_code| constains the error and
  // the |extra_code1| usually contains a system error, but it can contain
  // any extended information that is relevant to either the category or the
  // error itself.
  // The `installer_result_` contains the value provided by the `CrxInstaller`
  // instance when the install completes.
  ErrorCategory error_category_ = ErrorCategory::kNone;
  int error_code_ = 0;
  int extra_code1_ = 0;
  std::optional<CrxInstaller::Result> installer_result_;
  ErrorCategory diff_error_category_ = ErrorCategory::kNone;
  int diff_error_code_ = 0;
  int diff_extra_code1_ = 0;

  // Contains app-specific custom response attributes from the server, sent in
  // the last update check.
  std::map<std::string, std::string> custom_attrs_;

  // Contains the optional install parameters from the update response.
  std::optional<CrxInstaller::InstallParams> install_params_;

  // Contains the events which are therefore serialized in the requests.
  std::vector<base::Value::Dict> events_;

  CallbackHandleComplete callback_handle_complete_;
  std::unique_ptr<State> state_;
  const raw_ref<const UpdateContext> update_context_;

  ComponentState previous_state_ = ComponentState::kLastStatus;

  // True if this component has reached a final state because all its states
  // have been handled.
  bool is_handled_ = false;
};

using IdToComponentPtrMap = std::map<std::string, std::unique_ptr<Component>>;

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_H_
