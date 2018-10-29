// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_COMPONENT_H_
#define COMPONENTS_UPDATE_CLIENT_COMPONENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/update_client/crx_downloader.h"
#include "components/update_client/protocol_parser.h"
#include "components/update_client/update_client.h"
#include "url/gurl.h"

namespace base {
class Value;
}  // namespace base

namespace update_client {

class ActionRunner;
class Configurator;
struct CrxUpdateItem;
struct UpdateContext;

// Describes a CRX component managed by the UpdateEngine. Each |Component| is
// associated with an UpdateContext.
class Component {
 public:
  using Events = UpdateClient::Observer::Events;

  using CallbackHandleComplete = base::OnceCallback<void()>;

  Component(const UpdateContext& update_context, const std::string& id);
  ~Component();

  // Handles the current state of the component and makes it transition
  // to the next component state before |callback_handle_complete_| is invoked.
  void Handle(CallbackHandleComplete callback_handle_complete);

  CrxUpdateItem GetCrxUpdateItem() const;

  // Sets the uninstall state for this component.
  void Uninstall(const base::Version& cur_version, int reason);

  // Called by the UpdateEngine when an update check for this component is done.
  void SetUpdateCheckResult(
      const base::Optional<ProtocolParser::Result>& result,
      ErrorCategory error_category,
      int error);

  // Returns true if the component has reached a final state and no further
  // handling and state transitions are possible.
  bool IsHandled() const { return is_handled_; }

  // Returns true if an update is available for this component, meaning that
  // the update server has return a response containing an update.
  bool IsUpdateAvailable() const { return is_update_available_; }

  base::TimeDelta GetUpdateDuration() const;

  ComponentState state() const { return state_->state(); }

  std::string id() const { return id_; }

  const base::Optional<CrxComponent>& crx_component() const {
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

  bool diff_update_failed() const { return !!diff_error_code_; }

  ErrorCategory error_category() const { return error_category_; }
  int error_code() const { return error_code_; }
  int extra_code1() const { return extra_code1_; }
  ErrorCategory diff_error_category() const { return diff_error_category_; }
  int diff_error_code() const { return diff_error_code_; }
  int diff_extra_code1() const { return diff_extra_code1_; }

  std::string action_run() const { return action_run_; }

  scoped_refptr<Configurator> config() const;

  std::string session_id() const;

  const std::vector<base::Value>& events() const { return events_; }

  // Returns a clone of the component events.
  std::vector<base::Value> GetEvents() const;

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

   protected:
    // Initiates the transition to the new state.
    void TransitionState(std::unique_ptr<State> new_state);

    // Makes the current state a final state where no other state transition
    // can further occur.
    void EndState();

    Component& component() { return component_; }
    const Component& component() const { return component_; }

    base::ThreadChecker thread_checker_;

    const ComponentState state_;

   private:
    virtual void DoHandle() = 0;

    Component& component_;
    CallbackNextState callback_next_state_;
  };

  class StateNew : public State {
   public:
    explicit StateNew(Component* component);
    ~StateNew() override;

   private:
    // State overrides.
    void DoHandle() override;

    DISALLOW_COPY_AND_ASSIGN(StateNew);
  };

  class StateChecking : public State {
   public:
    explicit StateChecking(Component* component);
    ~StateChecking() override;

   private:
    // State overrides.
    void DoHandle() override;

    void UpdateCheckComplete();

    DISALLOW_COPY_AND_ASSIGN(StateChecking);
  };

  class StateUpdateError : public State {
   public:
    explicit StateUpdateError(Component* component);
    ~StateUpdateError() override;

   private:
    // State overrides.
    void DoHandle() override;

    DISALLOW_COPY_AND_ASSIGN(StateUpdateError);
  };

  class StateCanUpdate : public State {
   public:
    explicit StateCanUpdate(Component* component);
    ~StateCanUpdate() override;

   private:
    // State overrides.
    void DoHandle() override;
    bool CanTryDiffUpdate() const;

    DISALLOW_COPY_AND_ASSIGN(StateCanUpdate);
  };

  class StateUpToDate : public State {
   public:
    explicit StateUpToDate(Component* component);
    ~StateUpToDate() override;

   private:
    // State overrides.
    void DoHandle() override;

    DISALLOW_COPY_AND_ASSIGN(StateUpToDate);
  };

  class StateDownloadingDiff : public State {
   public:
    explicit StateDownloadingDiff(Component* component);
    ~StateDownloadingDiff() override;

   private:
    // State overrides.
    void DoHandle() override;

    // Called when progress is being made downloading a CRX. Can be called
    // multiple times due to how the CRX downloader switches between
    // different downloaders and fallback urls.
    void DownloadProgress(const std::string& id);

    void DownloadComplete(const std::string& id,
                          const CrxDownloader::Result& download_result);

    // Downloads updates for one CRX id only.
    std::unique_ptr<CrxDownloader> crx_downloader_;

    DISALLOW_COPY_AND_ASSIGN(StateDownloadingDiff);
  };

  class StateDownloading : public State {
   public:
    explicit StateDownloading(Component* component);
    ~StateDownloading() override;

   private:
    // State overrides.
    void DoHandle() override;

    // Called when progress is being made downloading a CRX. Can be called
    // multiple times due to how the CRX downloader switches between
    // different downloaders and fallback urls.
    void DownloadProgress(const std::string& id);

    void DownloadComplete(const std::string& id,
                          const CrxDownloader::Result& download_result);

    // Downloads updates for one CRX id only.
    std::unique_ptr<CrxDownloader> crx_downloader_;

    DISALLOW_COPY_AND_ASSIGN(StateDownloading);
  };

  class StateUpdatingDiff : public State {
   public:
    explicit StateUpdatingDiff(Component* component);
    ~StateUpdatingDiff() override;

   private:
    // State overrides.
    void DoHandle() override;

    void InstallComplete(ErrorCategory error_category,
                         int error_code,
                         int extra_code1);

    DISALLOW_COPY_AND_ASSIGN(StateUpdatingDiff);
  };

  class StateUpdating : public State {
   public:
    explicit StateUpdating(Component* component);
    ~StateUpdating() override;

   private:
    // State overrides.
    void DoHandle() override;

    void InstallComplete(ErrorCategory error_category,
                         int error_code,
                         int extra_code1);

    DISALLOW_COPY_AND_ASSIGN(StateUpdating);
  };

  class StateUpdated : public State {
   public:
    explicit StateUpdated(Component* component);
    ~StateUpdated() override;

   private:
    // State overrides.
    void DoHandle() override;

    DISALLOW_COPY_AND_ASSIGN(StateUpdated);
  };

  class StateUninstalled : public State {
   public:
    explicit StateUninstalled(Component* component);
    ~StateUninstalled() override;

   private:
    // State overrides.
    void DoHandle() override;

    DISALLOW_COPY_AND_ASSIGN(StateUninstalled);
  };

  class StateRun : public State {
   public:
    explicit StateRun(Component* component);
    ~StateRun() override;

   private:
    // State overrides.
    void DoHandle() override;

    void ActionRunComplete(bool succeeded, int error_code, int extra_code1);

    // Runs the action referred by the |action_run_| member of the Component
    // class.
    std::unique_ptr<ActionRunner> action_runner_;

    DISALLOW_COPY_AND_ASSIGN(StateRun);
  };

  // Returns true is the update payload for this component can be downloaded
  // by a downloader which can do bandwidth throttling on the client side.
  bool CanDoBackgroundDownload() const;

  void AppendEvent(base::Value event);

  // Changes the component state and notifies the caller of the |Handle|
  // function that the handling of this component state is complete.
  void ChangeState(std::unique_ptr<State> next_state);

  // Notifies registered observers about changes in the state of the component.
  void NotifyObservers(Events event) const;

  void SetParseResult(const ProtocolParser::Result& result);

  // These functions return a specific event. Each data member of the event is
  // represented as a key-value pair in a dictionary value.
  base::Value MakeEventUpdateComplete() const;
  base::Value MakeEventDownloadMetrics(
      const CrxDownloader::DownloadMetrics& download_metrics) const;
  base::Value MakeEventUninstalled() const;
  base::Value MakeEventActionRun(bool succeeded,
                                 int error_code,
                                 int extra_code1) const;

  base::ThreadChecker thread_checker_;

  const std::string id_;
  base::Optional<CrxComponent> crx_component_;

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

  base::FilePath crx_path_;

  // The error information for full and differential updates.
  // The |error_category| contains a hint about which module in the component
  // updater generated the error. The |error_code| constains the error and
  // the |extra_code1| usually contains a system error, but it can contain
  // any extended information that is relevant to either the category or the
  // error itself.
  ErrorCategory error_category_ = ErrorCategory::kNone;
  int error_code_ = 0;
  int extra_code1_ = 0;
  ErrorCategory diff_error_category_ = ErrorCategory::kNone;
  int diff_error_code_ = 0;
  int diff_extra_code1_ = 0;

  // Contains the events which are therefore serialized in the requests.
  std::vector<base::Value> events_;

  CallbackHandleComplete callback_handle_complete_;
  std::unique_ptr<State> state_;
  const UpdateContext& update_context_;

  base::OnceClosure update_check_complete_;

  ComponentState previous_state_ = ComponentState::kLastStatus;

  // True if this component has reached a final state because all its states
  // have been handled.
  bool is_handled_ = false;

  DISALLOW_COPY_AND_ASSIGN(Component);
};

using IdToComponentPtrMap = std::map<std::string, std::unique_ptr<Component>>;

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COMPONENT_H_
