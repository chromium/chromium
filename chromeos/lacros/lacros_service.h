// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_SERVICE_H_
#define CHROMEOS_LACROS_LACROS_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/component_export.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_threadsafe.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/token.h"
#include "chromeos/components/sensors/mojom/cros_sensor_service.mojom.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/automation.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/device_attributes.mojom.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "chromeos/crosapi/mojom/multi_capture_service.mojom.h"
#include "chromeos/crosapi/mojom/nonclosable_app_toast_service.mojom.h"
#include "chromeos/crosapi/mojom/one_drive_notification_service.mojom.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "chromeos/crosapi/mojom/volume_manager.mojom.h"
#include "chromeos/lacros/lacros_service_never_blocking_state.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/policy/core/common/policy_namespace.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace media {
namespace stable::mojom {
class StableVideoDecoderFactory;
}  // namespace stable::mojom
}  // namespace media

namespace chromeos {

class NativeThemeCache;
class SystemIdleCache;

// Forward declaration for class defined in .cc file that holds most of the
// business logic of this class.
class LacrosServiceNeverBlockingState;

// This class is responsible for receiving and routing mojo messages from
// ash-chrome via the mojo::Receiver |sequenced_state_.receiver_|. This class is
// responsible for sending and routing messages to ash-chrome via the
// mojo::Remote |sequenced_state_.crosapi_|. Messages are sent and
// received on a dedicated, never-blocking sequence to avoid deadlocks.
//
// This object is constructed, destroyed, and mostly used on an "affine
// sequence". For most intents and purposes, this is the main/UI thread.
//
// This class is a singleton but is not thread safe. Each method is individually
// documented with threading requirements.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosService {
 public:
  using ComponentPolicyMap =
      base::flat_map<policy::PolicyNamespace, base::Value>;
  class Observer {
   public:
    // Called when the new policy data is received from Ash.
    virtual void OnPolicyUpdated(
        const std::vector<uint8_t>& policy_fetch_response) {}

    // Called when policy fetch attempt is made in Ash.
    virtual void OnPolicyFetchAttempt() {}

    // Called when the new component policy is received from Ash.
    virtual void OnComponentPolicyUpdated(const ComponentPolicyMap& policy) {}

   protected:
    virtual ~Observer() = default;
  };

  // The getter is safe to call from all threads.
  //
  // This method returns nullptr very early or late in the application
  // lifecycle. We've chosen to have precise constructor/destructor timings
  // rather than rely on a lazy initializer and no destructor to allow for
  // more precise testing.
  //
  // If this is accessed on a thread other than the affine sequence, the caller
  // must invalidate or destroy the pointer before shutdown. Attempting to use
  // this pointer during shutdown can result in UaF.
  static LacrosService* Get();

  // This class is expected to be constructed and destroyed on the same
  // sequence.
  LacrosService();
  LacrosService(const LacrosService&) = delete;
  LacrosService& operator=(const LacrosService&) = delete;
  ~LacrosService();

  // This can be called on any thread. This call allows LacrosService
  // to start receiving messages from ash-chrome.
  // |browser_version| is the version of lacros-chrome displayed to user in
  // feedback report, etc.
  // It includes both browser version and channel in the format of:
  // {browser version} {channel}
  // For example, "87.0.0.1 dev", "86.0.4240.38 beta".
  void BindReceiver(const std::string& browser_version);

  // Methods to add/remove observer. Safe to call from any thread.
  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Notifies that the device account policy is updated with the input data
  // to observers. The data comes as serialized blob of PolicyFetchResponse
  // object.
  // This must be called on the affined sequence.
  void NotifyPolicyUpdated(const std::vector<uint8_t>& policy);

  // Notifies that an attempt to update the device account policy has been made.
  void NotifyPolicyFetchAttempt();

  // Notifies that the device account component policy is updated with the
  // input data. Must be called on the affined sequence.
  void NotifyComponentPolicyUpdated(ComponentPolicyMap policy);

  // Returns whether Ash supports that crosapi.
  template <typename CrosapiInterface>
  bool IsSupported() const {
    return GetInterfaceVersion<CrosapiInterface>() >= 0;
  }

  // Returns whether this interface uses the automatic registration system to be
  // available for immediate use at startup. Any crosapi interface can be
  // registered by using ConstructRemote.
  template <typename CrosapiInterface>
  bool IsRegistered() const {
    return base::Contains(interfaces_, CrosapiInterface::Uuid_);
  }

  // Guards usage to the corresponding crosapi interface. Can only be used with
  // automatically registered interfaces. See IsRegistered().
  template <typename CrosapiInterface>
  bool IsAvailable() const {
    DCHECK(IsRegistered<CrosapiInterface>());
    return interfaces_.find(CrosapiInterface::Uuid_)->second->IsAvailable();
  }

  // Returns the automatically registered remote for a given crosapi interface.
  // Can only be used with automatically registered features that are also
  // available. This method can only be called from the affine sequence (main
  // thread). The returned remote can only be used on the affine sequence (main
  // thread).
  // Note that the remote will not be owned by the caller, so callers should
  // avoid calling methods that would mutate the state of the remote such as
  // set_disconnect_handler, since only one can be set on a Remote.
  template <typename CrosapiInterface>
  mojo::Remote<CrosapiInterface>& GetRemote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsAvailable<CrosapiInterface>());
    return interfaces_.find(CrosapiInterface::Uuid_)
        ->second->template Get<CrosapiInterface>();
  }

  // --------------------------------------------------------------------------
  // Some clients will want to use mojo::Remotes on arbitrary sequences (e.g.
  // background threads). The following methods allow the client to construct a
  // mojo::Remote bound to an arbitrary sequence, and pass the other endpoint of
  // the Remote (mojo::PendingReceiver) to ash to set up the interface. For
  // other interfaces, such as media::stable::mojom::StableVideoDecoderFactory,
  // the main reason to use a Bind*() method instead of GetRemote() is not the
  // threading model, but the fact that the browser may want to maintain
  // multiple independent mojo::Remotes, and ash-chrome can use this behavior as
  // useful information (for example, to start one ash-chrome utility video
  // decoder process per lacros-chrome renderer process in order to host the
  // implementation of a media::stable::mojom::StableVideoDecoderFactory).
  // --------------------------------------------------------------------------

  // This may be called on any thread.
  void BindAccountManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver);

  // This may be called on any thread.
  void BindAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote);

  // This may be called on any thread.
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          remote);

  // This may be called on any thread.
  void BindBrowserCdmFactory(mojo::GenericPendingReceiver receiver);

  // This may be called on any thread.
  void BindCfmServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
          pending_receiver);

  // This may be called on any thread.
  void BindGeolocationService(
      mojo::PendingReceiver<crosapi::mojom::GeolocationService>
          pending_receiver);

  // This may be called on any thread.
  void BindMachineLearningService(
      mojo::PendingReceiver<
          chromeos::machine_learning::mojom::MachineLearningService> receiver);

  // This may be called on any thread.
  void BindMagicBoostController(
      mojo::PendingReceiver<crosapi::mojom::MagicBoostController> receiver);

  // Binds the mahi browser delegate to the mahi browser client.
  void BindMahiBrowserDelegate(
      mojo::PendingReceiver<crosapi::mojom::MahiBrowserDelegate> receiver);

  // This may be called on any thread.
  void BindMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          remote);

  // This may be called on any thread.
  void BindMetricsReporting(
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver);

  // This may be called on any thread.
  void BindRemoteAppsLacrosBridge(
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge> receiver);

  // This may be called on any thread.
  void BindPrintPreviewCrosDelegate(
      mojo::PendingReceiver<crosapi::mojom::PrintPreviewCrosDelegate> receiver);

  // This may be called on any thread.
  void BindScreenManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver);

  // This may be called on any thread.
  void BindSensorHalClient(
      mojo::PendingRemote<chromeos::sensors::mojom::SensorHalClient> remote);

  // This may be called on any thread.
  void BindMediaApp(mojo::PendingRemote<crosapi::mojom::MediaApp> remote);

  // OnLacrosStartup method of Crosapi can only be called if this method
  // returns true.
  bool IsOnBrowserStartupAvailable() const;

  // Binds video capture host.
  void BindVideoCaptureDeviceFactory(
      mojo::PendingReceiver<crosapi::mojom::VideoCaptureDeviceFactory>
          pending_receiver);

  // This may be called on any thread.
  void BindStableVideoDecoderFactory(
      mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
          receiver);

  // Binds video conference manager to lacros-browser clients.
  void BindVideoConferenceManager(
      mojo::PendingReceiver<crosapi::mojom::VideoConferenceManager> receiver);

  // Returns SystemIdleCache, which uses IdleInfoObserver to observe idle info
  // changes and caches the results. Requires IsIdleServiceAvailable() for full
  // function, and is robust against unavailability.
  SystemIdleCache* system_idle_cache() { return system_idle_cache_.get(); }

  // Returns the version for an ash interface with a given mojom interface,
  // or -1 if not found.
  // This is synchronous version of mojo::Remote::QueryVersion for crosapi
  // interfaces.
  //
  // Example code:
  //    LacrosService::Get()->GetInterfaceVersion<crosapi::mojom::Arc>();
  template <typename T>
  int GetInterfaceVersion() const {
    return GetInterfaceVersion(T::Uuid_);
  }

  // Prefer using GetInterfaceVersion<T>().
  int GetInterfaceVersion(base::Token interface_uuid) const;

  using Crosapi = crosapi::mojom::Crosapi;

  // This function binds a pending receiver or remote by posting the
  // corresponding bind task to the |never_blocking_sequence_|.
  // This method is public because not all clients can use the syntax sugar of
  // ConstructRemote(), which relies on the assumption that each crosapi
  // interface only has a single associated Bind* method.
  template <typename PendingReceiverOrRemote,
            void (Crosapi::*bind_func)(PendingReceiverOrRemote)>
  void BindPendingReceiverOrRemote(
      PendingReceiverOrRemote pending_receiver_or_remote) {
    never_blocking_sequence_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &LacrosServiceNeverBlockingState::BindCrosapiFeatureReceiver<
                PendingReceiverOrRemote, bind_func>,
            weak_sequenced_state_, std::move(pending_receiver_or_remote)));
  }

  // Injects remote for a registered crosapi interface.
  template <typename CrosapiInterface>
  void InjectRemoteForTesting(
      mojo::PendingRemote<CrosapiInterface> pending_remote) {
    DCHECK(IsRegistered<CrosapiInterface>());
    did_bind_receiver_ = true;

    interfaces_.find(CrosapiInterface::Uuid_)
        ->second->InjectRemoteForTesting(std::move(pending_remote));
  }

 private:
  // This class is a wrapper around a crosapi remote, e.g.
  // mojo::Remote<crosapi::mojom::Automation>. This base class uses type erasure
  // to allow us to store all instances in a single container.
  class InterfaceEntryBase {
   public:
    virtual ~InterfaceEntryBase();

    // Returns the remote that is being wrapped.
    template <typename CrosapiInterface>
    mojo::Remote<CrosapiInterface>& Get() {
      return *reinterpret_cast<mojo::Remote<CrosapiInterface>*>(GetInternal());
    }

    // Returns whether Ash is sufficiently recent to support the crosapi
    // protocol that the remote is based on.
    bool IsAvailable() const { return available_; }

    // Initialization for the remote and |available_|.
    virtual void MaybeBind(LacrosService* impl) = 0;

    template <typename CrosapiInterface>
    void InjectRemoteForTesting(
        mojo::PendingRemote<CrosapiInterface> pending_remote) {
      available_ = pending_remote.is_valid();
      Get<CrosapiInterface>() = mojo::Remote(std::move(pending_remote));
    }

   protected:
    InterfaceEntryBase();
    InterfaceEntryBase(const InterfaceEntryBase&) = delete;
    InterfaceEntryBase& operator=(const InterfaceEntryBase&) = delete;

    // Returns a raw pointer to a mojo::Remote<CrosapiInterface>.
    virtual void* GetInternal() = 0;

    // See |IsAvailable|.
    bool available_ = false;
  };

  // LacrosServiceNeverBlockingState is an implementation detail of
  // this class.
  friend class LacrosServiceNeverBlockingState;

  FRIEND_TEST_ALL_PREFIXES(LacrosServiceTest, CheckCrosapiRemoteVersion);

  // Forward declare inner class to give it access to private members.
  template <typename CrosapiInterface,
            void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
            uint32_t MethodMinVersion>
  class InterfaceEntry;

  // Returns ash's version of the Crosapi mojo interface version. This
  // determines which interface methods are available. This is safe to call from
  // any sequence. This can only be called after BindReceiver().
  std::optional<uint32_t> CrosapiVersion() const;

  // Requests ash-chrome to send idle info updates.
  void StartSystemIdleCache();

  // Requests ash-chrome to send native theme info updates.
  void StartNativeThemeCache();

  // This function initializes a remote for a given CrosapiInterface. Returns
  // true if remote initialization succeeds; otherwise, returns false.
  template <typename CrosapiInterface,
            void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>)>
  bool MaybeInitializeAndBindRemote(mojo::Remote<CrosapiInterface>* remote) {
    const int version = GetInterfaceVersion<CrosapiInterface>();
    if (version < 0) {
      return false;
    }

    // Implement the same functionality as
    // `mojo::Remote::BindNewPipeAndPassReceiver()`, but explicitly set the
    // remote version.
    mojo::MessagePipe pipe;
    remote->Bind(
        mojo::PendingRemote<CrosapiInterface>(std::move(pipe.handle0), version),
        /*task_runner=*/nullptr);

    BindPendingReceiverOrRemote<mojo::PendingReceiver<CrosapiInterface>,
                                bind_func>(
        mojo::PendingReceiver<CrosapiInterface>(std::move(pipe.handle1)));
    return true;
  }

  // This function constructs a new remote for a crosapi interface and stashes
  // it in |interfaces_|. This remote will later be bound during BindReceiver().
  template <typename CrosapiInterface,
            void (Crosapi::*bind_func)(mojo::PendingReceiver<CrosapiInterface>),
            uint32_t MethodMinVersion>
  void ConstructRemote();

  // Receiver and cache of system idle info updates.
  std::unique_ptr<SystemIdleCache> system_idle_cache_;

  // Receiver and cache of native theme info updates.
  std::unique_ptr<NativeThemeCache> native_theme_cache_;

  // A sequence that is guaranteed to never block.
  scoped_refptr<base::SequencedTaskRunner> never_blocking_sequence_;

  // This member is instantiated on the affine sequence alongside the
  // constructor. All subsequent invocations of this member, including
  // destruction, happen on the |never_blocking_sequence_|.
  std::unique_ptr<LacrosServiceNeverBlockingState, base::OnTaskRunnerDeleter>
      sequenced_state_;

  // This member is instantiated on the affine sequence, but only ever
  // dereferenced on the |never_blocking_sequence_|.
  base::WeakPtr<LacrosServiceNeverBlockingState> weak_sequenced_state_;

  // Set to true after BindReceiver() is called.
  bool did_bind_receiver_ = false;

  // The list of observers.
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;

  // Each element of |interfaces_| corresponds to a crosapi interface remote
  // (e.g. mojo::Remote<crosapi::mojom::Automation>). The key of the element is
  // the UUID of the crosapi interface. The value is a wrapper around the
  // mojo::Remote. Each element can only be used on the affine sequence. Each
  // element is automatically bound to the corresponding receiver in ash.
  std::map<base::Token, std::unique_ptr<InterfaceEntryBase>> interfaces_;

  // Checks that the method is called on the affine sequence.
  SEQUENCE_CHECKER(affine_sequence_checker_);

  base::WeakPtrFactory<LacrosService> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_SERVICE_H_
