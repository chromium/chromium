// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_
#define CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "chromeos/crosapi/mojom/cert_database.mojom.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/metrics_reporting.mojom.h"
#include "chromeos/crosapi/mojom/prefs.mojom.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "chromeos/crosapi/mojom/select_file.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/hid.mojom.h"

class GURL;

namespace chromeos {

class LacrosChromeServiceDelegate;

// Forward declaration for class defined in .cc file that holds most of the
// business logic of this class.
class LacrosChromeServiceNeverBlockingState;

// This class is responsible for receiving and routing mojo messages from
// ash-chrome via the mojo::Receiver |sequenced_state_.receiver_|. This class is
// responsible for sending and routing messages to ash-chrome via the
// mojo::Remote |sequenced_state_.ash_chrome_service_|. Messages are sent and
// received on a dedicated, never-blocking sequence to avoid deadlocks.
//
// This object is constructed, destroyed, and mostly used on an "affine
// sequence". For most intents and purposes, this is the main/UI thread.
//
// This class is a singleton but is not thread safe. Each method is individually
// documented with threading requirements.
class COMPONENT_EXPORT(CHROMEOS_LACROS) LacrosChromeServiceImpl {
 public:
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
  static LacrosChromeServiceImpl* Get();

  // This class is expected to be constructed and destroyed on the same
  // sequence.
  explicit LacrosChromeServiceImpl(
      std::unique_ptr<LacrosChromeServiceDelegate> delegate);
  ~LacrosChromeServiceImpl();

  // This can be called on any thread. This call allows LacrosChromeServiceImpl
  // to start receiving messages from ash-chrome.
  void BindReceiver(
      mojo::PendingReceiver<crosapi::mojom::LacrosChromeService> receiver);

  // Called during tests on affine sequence to disable all crosapi
  // functionality.
  // TODO(https://crbug.com/1131722): Ideally we could stub this out or make
  // this functional for tests without modifying production code
  static void DisableCrosapiForTests();

  // --------------------------------------------------------------------------
  // mojo::Remote is sequence affine. The following methods are convenient
  // helpers that expose pre-established Remotes that can only be used from the
  // affine sequence (main thread).
  // --------------------------------------------------------------------------

  // message_center_remote() can only be used if this method returns true.
  bool IsMessageCenterAvailable();

  // This must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::MessageCenter>& message_center_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsMessageCenterAvailable());
    return message_center_remote_;
  }

  // select_file_remote() can only be used if this method returns true.
  bool IsSelectFileAvailable();

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to show a select-file dialog.
  mojo::Remote<crosapi::mojom::SelectFile>& select_file_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsSelectFileAvailable());
    return select_file_remote_;
  }

  // keystore_service_remote() can only be used if this method returns true.
  bool IsKeystoreServiceAvailable();

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to query the system keystores.
  mojo::Remote<crosapi::mojom::KeystoreService>& keystore_service_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsKeystoreServiceAvailable());
    return keystore_service_remote_;
  }

  // hid_manager_remote() can only be used if this method returns true.
  bool IsHidManagerAvailable();

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to support HID devices.
  mojo::Remote<device::mojom::HidManager>& hid_manager_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsHidManagerAvailable());
    return hid_manager_remote_;
  }

  // feedback_remote() can only be used when this method returns true;
  bool IsFeedbackAvailable();

  // This must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::Feedback>& feedback_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsFeedbackAvailable());
    return feedback_remote_;
  }

  // media_session_audio_focus_remote() can only be used when this method
  // returns true;
  bool IsMediaSessionAudioFocusAvailable();

  // This must be called on the affine sequence.
  void BindAudioFocusManager(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManager> remote);

  // media_session_audio_focus_debug_remote() can only be used when this method
  // returns true;
  bool IsMediaSessionAudioFocusDebugAvailable();

  // This must be called on the affine sequence.
  void BindAudioFocusManagerDebug(
      mojo::PendingReceiver<media_session::mojom::AudioFocusManagerDebug>
          remote);

  // media_session_controller_remote() can only be used when this method returns
  // true;
  bool IsMediaSessionControllerAvailable();

  // This must be called on the affine sequence.
  void BindMediaControllerManager(
      mojo::PendingReceiver<media_session::mojom::MediaControllerManager>
          remote);

  // Whether the MetricsReporting API is available.
  bool IsMetricsReportingAvailable();

  // Binds a receiver for the MetricsReporting API. May be called on any thread.
  void BindMetricsReporting(
      mojo::PendingReceiver<crosapi::mojom::MetricsReporting> receiver);

  // cert_database_remote() can only be used when this method returns true;
  bool IsCertDbAvailable();

  // This must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::CertDatabase>& cert_database_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsCertDbAvailable());
    return cert_database_remote_;
  }

  // file_manager_remote() can only be used if this method returns true.
  bool IsFileManagerAvailable();

  // Must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::FileManager>& file_manager_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsFileManagerAvailable());
    return file_manager_remote_;
  }

  // test_controller_remote() can only be used if this method returns true.
  bool IsTestControllerAvailable();

  // Must be called on the affine sequence.
  mojo::Remote<crosapi::mojom::TestController>& test_controller_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsTestControllerAvailable());
    return test_controller_remote_;
  }

  // clipboard_remote() can only be used if this method returns true.
  bool IsClipboardAvailable();

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to interface with the clipboard
  mojo::Remote<crosapi::mojom::Clipboard>& clipboard_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsClipboardAvailable());
    return clipboard_remote_;
  }

  // Whether the Prefs API is available.
  bool IsPrefsAvailable();

  // This must be called on the affine sequence. It exposes a remote that can
  // be used to interface with Prefs.
  mojo::Remote<crosapi::mojom::Prefs>& prefs_remote() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(affine_sequence_checker_);
    DCHECK(IsPrefsAvailable());
    return prefs_remote_;
  }

  // --------------------------------------------------------------------------
  // Some clients will want to use mojo::Remotes on arbitrary sequences (e.g.
  // background threads). The following methods allow the client to construct a
  // mojo::Remote bound to an arbitrary sequence, and pass the other endpoint of
  // the Remote (mojo::PendingReceiver) to ash to set up the interface.
  // --------------------------------------------------------------------------

  // BindScreenManagerReceiver() can only be used if this method returns true.
  bool IsScreenManagerAvailable();

  // This may be called on any thread.
  void BindScreenManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::ScreenManager> pending_receiver);

  // BindAccountManagerReceiver() can only be used if this method returns true.
  bool IsAccountManagerAvailable();

  // This may be called on any thread.
  void BindAccountManagerReceiver(
      mojo::PendingReceiver<crosapi::mojom::AccountManager> pending_receiver);

  // OnLacrosStartup method of AshChromeService crosapi can only be called
  // if this method returns true.
  bool IsOnLacrosStartupAvailable();

  // Returns LacrosInitParams which is passed from ash-chrome. On launching
  // lacros-chrome from ash-chrome, ash-chrome creates a memory backed file
  // serializes the LacrosInitParams to it, and the forked/executed
  // lacros-chrome process inherits the file descriptor. The data is read
  // in the constructor so is available from the beginning.
  // Note that, in older versions, ash-chrome passes the data via
  // LacrosChromeService::Init() mojo call to lacros-chrome. That case is still
  // handled for backward compatibility, and planned to be removed in the
  // future (crbug.com/1156033). Though, until the removal, it is recommended
  // to consider both cases, specifically, at least not to cause a crash.
  const crosapi::mojom::LacrosInitParams* init_params() const {
    return init_params_.get();
  }

  // Returns the version for an ash interface with a given UUID. Returns -1 if
  // the interface is not found. This is a synchronous version of
  // mojo::Remote::QueryVersion. It relies on Ash M88. Features that need to
  // work on M87 or older should not use this.
  int GetInterfaceVersion(base::Token interface_uuid) const;

  // Sets `init_params_` to the provided value.
  // Useful for tests that cannot setup a full Lacros test environment with a
  // working Mojo connection to Ash.
  void SetInitParamsForTests(crosapi::mojom::LacrosInitParamsPtr init_params);

 private:
  // LacrosChromeServiceNeverBlockingState is an implementation detail of this
  // class.
  friend class LacrosChromeServiceNeverBlockingState;

  // Creates a new window on the affine sequence.
  void NewWindowAffineSequence();

  using GetFeedbackDataCallback = base::OnceCallback<void(base::Value)>;
  // Gets feedback data on the affine sequence.
  void GetFeedbackDataAffineSequence(GetFeedbackDataCallback callback);

  using GetHistogramsCallback = base::OnceCallback<void(const std::string&)>;
  // Gets histograms on the affine sequence.
  void GetHistogramsAffineSequence(GetHistogramsCallback callback);

  using GetActiveTabUrlCallback =
      base::OnceCallback<void(const base::Optional<GURL>&)>;
  // Gets Url of the active tab on the affine sequence.
  void GetActiveTabUrlAffineSequence(GetActiveTabUrlCallback callback);

  // Returns ash's version of the AshChromeService mojo interface version. This
  // determines which interface methods are available. This is safe to call from
  // any sequence. This can only be called after BindReceiver().
  base::Optional<uint32_t> AshChromeServiceVersion();

  // Delegate instance to inject Chrome dependent code. Must only be used on the
  // affine sequence.
  std::unique_ptr<LacrosChromeServiceDelegate> delegate_;

  // Parameters passed from ash-chrome.
  crosapi::mojom::LacrosInitParamsPtr init_params_;

  // These members are affine to the affine sequence. They are initialized in
  // the constructor and are immediately available for use.
  mojo::Remote<crosapi::mojom::MessageCenter> message_center_remote_;
  mojo::Remote<crosapi::mojom::SelectFile> select_file_remote_;
  mojo::Remote<device::mojom::HidManager> hid_manager_remote_;
  mojo::Remote<crosapi::mojom::Feedback> feedback_remote_;
  mojo::Remote<crosapi::mojom::CertDatabase> cert_database_remote_;
  mojo::Remote<crosapi::mojom::KeystoreService> keystore_service_remote_;
  mojo::Remote<crosapi::mojom::FileManager> file_manager_remote_;
  mojo::Remote<crosapi::mojom::TestController> test_controller_remote_;
  mojo::Remote<crosapi::mojom::Clipboard> clipboard_remote_;
  mojo::Remote<crosapi::mojom::Prefs> prefs_remote_;

  // This member is instantiated on the affine sequence alongside the
  // constructor. All subsequent invocations of this member, including
  // destruction, happen on the |never_blocking_sequence_|.
  std::unique_ptr<LacrosChromeServiceNeverBlockingState,
                  base::OnTaskRunnerDeleter>
      sequenced_state_;

  // This member is instantiated on the affine sequence, but only ever
  // dereferenced on the |never_blocking_sequence_|.
  base::WeakPtr<LacrosChromeServiceNeverBlockingState> weak_sequenced_state_;

  // A sequence that is guaranteed to never block.
  scoped_refptr<base::SequencedTaskRunner> never_blocking_sequence_;

  // Set to true after BindReceiver() is called.
  bool did_bind_receiver_ = false;

  // Checks that the method is called on the affine sequence.
  SEQUENCE_CHECKER(affine_sequence_checker_);

  base::WeakPtrFactory<LacrosChromeServiceImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_LACROS_CHROME_SERVICE_IMPL_H_
