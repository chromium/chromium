// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#if defined(OS_WIN)
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"
#endif  // defined(OS_WIN)
#include "chrome/browser/vr/test/browser_test_browser_renderer_browser_interface.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/xr_browser_test.h"

namespace vr {

UiUtils::UiUtils()
    : ui_operation_results_(std::vector<UiTestOperationResult>(
          static_cast<int>(UiTestOperationType::kNumUiTestOperationTypes))),
      ui_operation_callbacks_(std::vector<base::OnceCallback<void()>>(
          static_cast<int>(UiTestOperationType::kNumUiTestOperationTypes))),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  auto* renderer = GetBrowserRenderer();
  DCHECK(renderer) << "Failed to get a BrowserRenderer. Consider using "
                   << "UiUtils::Create() instead.";

  interface_ =
      std::make_unique<BrowserTestBrowserRendererBrowserInterface>(this);
  renderer->SetBrowserRendererBrowserInterfaceForTesting(interface_.get());
}

UiUtils::~UiUtils() {
  auto* renderer = GetBrowserRenderer();
  if (renderer != nullptr) {
    renderer->SetBrowserRendererBrowserInterfaceForTesting(nullptr);
  }
}

std::unique_ptr<UiUtils> UiUtils::Create() {
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&UiUtils::PollForBrowserRenderer, &wait_loop));
  wait_loop.Run();

  return std::make_unique<UiUtils>();
}

void UiUtils::PollForBrowserRenderer(base::RunLoop* wait_loop) {
  if (GetBrowserRenderer() == nullptr) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::BindOnce(&UiUtils::PollForBrowserRenderer, wait_loop),
        XrBrowserTestBase::kPollCheckIntervalShort);
    return;
  }
  wait_loop->Quit();
}

void UiUtils::PerformActionAndWaitForVisibilityStatus(
    const UserFriendlyElementName& element_name,
    const bool& visible,
    base::OnceCallback<void()> action) {
  ui_operation_results_[static_cast<int>(
      UiTestOperationType::kElementVisibilityStatus)] =
      UiTestOperationResult::kUnreported;
  base::RunLoop wait_loop(base::RunLoop::Type::kNestableTasksAllowed);
  ui_operation_callbacks_[static_cast<int>(
      UiTestOperationType::kElementVisibilityStatus)] =
      base::BindOnce([](base::RunLoop* loop) { loop->Quit(); }, &wait_loop);

  VisibilityChangeExpectation visibility_expectation;
  visibility_expectation.element_name = element_name;
  visibility_expectation.timeout_ms = kDefaultUiQuiescenceTimeout;
  visibility_expectation.visibility = visible;

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UiUtils::WatchElementForVisibilityStatusForTesting,
                     base::Unretained(this), visibility_expectation));

  wait_loop.Run();

  auto result = ui_operation_results_[static_cast<int>(
      UiTestOperationType::kElementVisibilityStatus)];
  CHECK(result == UiTestOperationResult::kVisibilityMatch)
      << "UI reported non-visibility-matched result '"
      << UiTestOperationResultToString(result) << "'";
}

void UiUtils::WatchElementForVisibilityStatusForTesting(
    VisibilityChangeExpectation visibility_expectation) {
  BrowserRenderer* browser_renderer = UiUtils::GetBrowserRenderer();
  if (browser_renderer) {
    interface_ =
        std::make_unique<BrowserTestBrowserRendererBrowserInterface>(this);
    browser_renderer->SetBrowserRendererBrowserInterfaceForTesting(
        interface_.get());
    browser_renderer->WatchElementForVisibilityStatusForTesting(
        visibility_expectation);
  }
}

void UiUtils::ReportUiOperationResult(const UiTestOperationType& action_type,
                                      const UiTestOperationResult& result) {
  ui_operation_results_[static_cast<int>(action_type)] = result;
  std::move(ui_operation_callbacks_[static_cast<int>(action_type)]).Run();
}

void UiUtils::DisableFrameTimeoutForTesting() {
#if defined(OS_WIN)
  VRBrowserRendererThreadWin::DisableFrameTimeoutForTesting();
#else
  NOTREACHED();
#endif  // defined(OS_WIN)
}

std::string UiUtils::UiTestOperationResultToString(
    UiTestOperationResult& result) {
  switch (result) {
    case UiTestOperationResult::kUnreported:
      return "Unreported";
    case UiTestOperationResult::kQuiescent:
      return "Quiescent";
    case UiTestOperationResult::kTimeoutNoStart:
      return "Timeout (UI activity not started)";
    case UiTestOperationResult::kTimeoutNoEnd:
      return "Timeout (UI activity not stopped)";
    case UiTestOperationResult::kVisibilityMatch:
      return "Visibility match";
    case UiTestOperationResult::kTimeoutNoVisibilityMatch:
      return "Timeout (Element visibility did not match)";
  }
}

VRBrowserRendererThreadWin* UiUtils::GetRendererThread() {
#if defined(OS_WIN)
  return VRBrowserRendererThreadWin::GetInstanceForTesting();
#else
  NOTREACHED();
#endif  // defined(OS_WIN)
}

BrowserRenderer* UiUtils::GetBrowserRenderer() {
#if defined(OS_WIN)
  auto* renderer_thread = GetRendererThread();
  if (renderer_thread == nullptr)
    return nullptr;
  return static_cast<VRBrowserRendererThreadWin*>(renderer_thread)
      ->GetBrowserRendererForTesting();
#else
  NOTREACHED();
#endif  // defined(OS_WIN)
}

}  // namespace vr
