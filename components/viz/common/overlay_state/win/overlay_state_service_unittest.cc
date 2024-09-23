// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/viz/common/overlay_state/win/overlay_state_service.h"
#include "components/viz/test/viz_test_suite.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/gpu_channel.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Combine;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::Values;

namespace viz {

class OverlayStateServiceUnittest : public testing::Test,
                                    public gpu::mojom::OverlayStateObserver {
 public:
  OverlayStateServiceUnittest()
      : receiver_(this), other_thread_("Other Thread") {}
  // PromotionHintObserver
  void OnStateChanged(bool promoted) override;
  void SetUp() override;

 protected:
  void PerformRegistration(const gpu::Mailbox& mailbox);
  void SetService();

  // Helper functions for running SetPromotionHint & MailboxDestroyed calls to
  // the OverlayStateService on a separate task sequence.
  base::OnceClosure SetPromotionHintClosure(const gpu::Mailbox& mailbox,
                                            bool promoted);
  void SetPromotionHint(const gpu::Mailbox& mailbox, bool promoted);
  void SetPromotionHintWorker(const gpu::Mailbox& mailbox, bool promoted);
  base::OnceClosure DestroyMailboxClosure(const gpu::Mailbox& mailbox);
  void DestroyMailbox(const gpu::Mailbox& mailbox);
  void DestroyMailboxWorker(const gpu::Mailbox& mailbox);

  // In order to simulate the OverlayStateService running on GpuMain and the
  // DCLayerOverlayProcessor running on VizCompositorMain we create a separate
  // thread & task runner where SetPromotionHint & DestroyMailbox calls are
  // executed from.
  scoped_refptr<base::SingleThreadTaskRunner> other_thread_task_runner_;

  raw_ptr<OverlayStateService> service_;
  mojo::Receiver<gpu::mojom::OverlayStateObserver> receiver_;
  int hint_received_ = 0;
  bool last_promote_value_ = false;

 private:
  base::Thread other_thread_;
};

void OverlayStateServiceUnittest::SetUp() {
  other_thread_.StartAndWaitForTesting();
  other_thread_task_runner_ = other_thread_.task_runner();
}

void OverlayStateServiceUnittest::OnStateChanged(bool promoted) {
  hint_received_++;
  last_promote_value_ = promoted;
}

void OverlayStateServiceUnittest::PerformRegistration(
    const gpu::Mailbox& mailbox) {
  service_->RegisterObserver(receiver_.BindNewPipeAndPassRemote(), mailbox);
}

void OverlayStateServiceUnittest::SetService() {
  service_ = OverlayStateService::GetInstance();
  if (!service_->IsInitialized()) {
    service_->Initialize(base::SequencedTaskRunner::GetCurrentDefault());
  }
}

void OverlayStateServiceUnittest::SetPromotionHint(const gpu::Mailbox& mailbox,
                                                   bool promoted) {
  base::RunLoop run_loop;
  other_thread_task_runner_->PostTask(
      FROM_HERE, SetPromotionHintClosure(std::move(mailbox), promoted));
  other_thread_task_runner_->PostTask(FROM_HERE,
                                      base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

base::OnceClosure OverlayStateServiceUnittest::SetPromotionHintClosure(
    const gpu::Mailbox& mailbox,
    bool promoted) {
  return base::BindOnce(&OverlayStateServiceUnittest::SetPromotionHintWorker,
                        base::Unretained(this), std::move(mailbox), promoted);
}

void OverlayStateServiceUnittest::SetPromotionHintWorker(
    const gpu::Mailbox& mailbox,
    bool promoted) {
  service_->SetPromotionHint(mailbox, promoted);
}

void OverlayStateServiceUnittest::DestroyMailbox(const gpu::Mailbox& mailbox) {
  base::RunLoop run_loop;
  other_thread_task_runner_->PostTask(
      FROM_HERE, DestroyMailboxClosure(std::move(mailbox)));
  other_thread_task_runner_->PostTask(FROM_HERE,
                                      base::BindOnce(run_loop.QuitClosure()));
  run_loop.Run();
}

base::OnceClosure OverlayStateServiceUnittest::DestroyMailboxClosure(
    const gpu::Mailbox& mailbox) {
  return base::BindOnce(&OverlayStateServiceUnittest::DestroyMailboxWorker,
                        base::Unretained(this), std::move(mailbox));
}

void OverlayStateServiceUnittest::DestroyMailboxWorker(
    const gpu::Mailbox& mailbox) {
  service_->MailboxDestroyed(mailbox);
}

TEST_F(OverlayStateServiceUnittest, ServiceSingleton) {
  OverlayStateService* service = OverlayStateService::GetInstance();
  EXPECT_NE(service, nullptr);

  OverlayStateService* service_instance_2 = OverlayStateService::GetInstance();
  EXPECT_EQ(service, service_instance_2);
}

TEST_F(OverlayStateServiceUnittest, AddObserver) {
  SetService();

  // Add observer for a new mailbox
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  PerformRegistration(mailbox);

  // Add observer for existing mailbox with set promotion state
  gpu::Mailbox mailbox2 = gpu::Mailbox::Generate();
  SetPromotionHint(mailbox2, true);
  VizTestSuite::RunUntilIdle();
  receiver_.reset();
  PerformRegistration(mailbox2);
  VizTestSuite::RunUntilIdle();
  EXPECT_EQ(hint_received_, 1);
  EXPECT_EQ(last_promote_value_, true);
}

TEST_F(OverlayStateServiceUnittest, SetHint) {
  SetService();
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  PerformRegistration(mailbox);
  VizTestSuite::RunUntilIdle();
  SetPromotionHint(mailbox, true);
  VizTestSuite::RunUntilIdle();
  EXPECT_EQ(hint_received_, 1);
  EXPECT_EQ(last_promote_value_, true);
  SetPromotionHint(mailbox, false);
  VizTestSuite::RunUntilIdle();
  EXPECT_EQ(hint_received_, 2);
  EXPECT_EQ(last_promote_value_, false);
}

TEST_F(OverlayStateServiceUnittest, DeleteMailbox) {
  SetService();
  gpu::Mailbox mailbox = gpu::Mailbox::Generate();
  PerformRegistration(mailbox);
  VizTestSuite::RunUntilIdle();
  SetPromotionHint(mailbox, true);
  VizTestSuite::RunUntilIdle();
  EXPECT_EQ(hint_received_, 1);
  EXPECT_EQ(last_promote_value_, true);
  // Tell the OverlayStateService the mailbox has been destroyed, but
  // don't actually destroy the mailbox for testing purposes as we want to
  // ensure another mailbox with the same identifier is treated as a separate
  // entity.
  DestroyMailbox(mailbox);
  // Send another promotion hint for the mailbox - we expect that we will not
  // receive a hint changed callback this time because when the mailbox was
  // "destroyed" any registered observers should have been removed.
  SetPromotionHint(mailbox, false);
  VizTestSuite::RunUntilIdle();
  EXPECT_EQ(hint_received_, 1);
  EXPECT_EQ(last_promote_value_, true);
}

}  // namespace viz
