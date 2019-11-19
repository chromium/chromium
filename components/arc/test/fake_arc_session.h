// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
#define COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/arc_stop_reason.h"

namespace arc {

// A fake ArcSession that creates a local connection.
class FakeArcSession : public ArcSession {
 public:
  FakeArcSession();
  ~FakeArcSession() override;

  // ArcSession overrides:
  void StartMiniInstance() override;
  void RequestUpgrade(UpgradeParams params) override;
  void Stop() override;
  bool IsStopRequested() override;
  void OnShutdown() override;
  void SetUserInfo(const std::string& hash,
                   const std::string& serial_number) override;

  // To emulate unexpected stop, such as crash.
  void StopWithReason(ArcStopReason reason);

  // The following control Start() behavior for testing various situations.

  // Enables/disables boot failure emulation, in which OnSessionStopped(reason)
  // will be called when Start() or StartForLoginScreen() is called.
  void EnableBootFailureEmulation(ArcStopReason reason);

  // Emulate Start() is suspended at some phase.
  void SuspendBoot();

  // To emulate the mini-container starting. This can cause a failure if
  // EnableBootFailureEmulation was called on this instance
  void EmulateMiniContainerStart();

  // Returns true if the session is considered as running.
  bool is_running() const { return running_; }

  // Returns an upgrade parameter passed to the session.
  std::string upgrade_locale_param() const { return upgrade_locale_param_; }

  // Returns FakeArcSession instance. This can be used for a factory
  // in ArcBridgeServiceImpl.
  static std::unique_ptr<ArcSession> Create();

 private:
  bool boot_failure_emulation_enabled_ = false;
  ArcStopReason boot_failure_reason_;

  bool boot_suspended_ = false;
  bool upgrade_requested_ = false;
  bool running_ = false;
  bool stop_requested_ = false;
  std::string upgrade_locale_param_;

  DISALLOW_COPY_AND_ASSIGN(FakeArcSession);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
