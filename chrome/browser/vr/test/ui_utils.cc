// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_utils.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "chrome/browser/vr/vr_browser_renderer_thread.h"
#include "chrome/browser/vr/test/xr_browser_test.h"

namespace vr {

UiUtils::UiUtils()
    : main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {
  auto* renderer = GetBrowserRenderer();
  DCHECK(renderer) << "Failed to get a BrowserRenderer. Consider using "
                   << "UiUtils::Create() instead.";
}

UiUtils::~UiUtils() {
  auto* renderer = GetBrowserRenderer();
  if (renderer != nullptr) {
    renderer->WatchElementForVisibilityStatusForTesting(std::nullopt);
  }
}

std::unique_ptr<UiUtils> UiUtils::Create() {
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UiUtils::PollForBrowserRenderer, &wait_loop));
  wait_loop.Run();

  return std::make_unique<UiUtils>();
}

void UiUtils::PollForBrowserRenderer(base::RunLoop* wait_loop) {
  if (GetBrowserRenderer() == nullptr) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&UiUtils::PollForBrowserRenderer, wait_loop),
        XrBrowserTestBase::kPollCheckIntervalShort);
    return;
  }
  wait_loop->Quit();
}

void UiUtils::WaitForVisibilityStatus(
    const UserFriendlyElementName& element_name,
    const bool& visible) {
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);

  std::optional<UiVisibilityState> visibility_expectation =
      std::make_optional<UiVisibilityState>();
  visibility_expectation->element_to_watch = element_name;
  visibility_expectation->timeout_ms =
      base::Milliseconds(kDefaultUiQuiescenceTimeout);
  visibility_expectation->expected_visibile = visible;
  visibility_expectation->on_visibility_change_result = base::BindOnce(
      [](base::RunLoop* loop, bool visibility_matched) {
        CHECK(visibility_matched)
            << "Ui reported non-visibility-matched result";
        loop->Quit();
      },
      &wait_loop);

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UiUtils::WatchElementForVisibilityStatusForTesting,
                     base::Unretained(this),
                     std::move(visibility_expectation)));

  wait_loop.Run();
}

void UiUtils::WatchElementForVisibilityStatusForTesting(
    std::optional<UiVisibilityState> visibility_expectation) {
  BrowserRenderer* browser_renderer = UiUtils::GetBrowserRenderer();
  if (browser_renderer) {
    // Reset the start time to now so that we don't count the time it took to
    // potentially post this task in the timeout.
    if (visibility_expectation.has_value()) {
      visibility_expectation->start_time = base::TimeTicks::Now();
    }
    browser_renderer->WatchElementForVisibilityStatusForTesting(
        std::move(visibility_expectation));
  }
}

void UiUtils::DisableOverlayForTesting() {
  VRBrowserRendererThread::DisableOverlayForTesting();
}

VRBrowserRendererThread* UiUtils::GetRendererThread() {
  return VRBrowserRendererThread::GetInstanceForTesting();
}

BrowserRenderer* UiUtils::GetBrowserRenderer() {
  auto* renderer_thread = GetRendererThread();
  if (renderer_thread == nullptr)
    return nullptr;
  return static_cast<VRBrowserRendererThread*>(renderer_thread)
      ->GetBrowserRendererForTesting();
}

}  // namespace vr
