// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/mechanisms/tab_loading_frame_navigation_scheduler.h"

#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "components/performance_manager/public/graph/policies/tab_loading_frame_navigation_policy.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/performance_manager_main_thread_mechanism.h"
#include "components/performance_manager/public/performance_manager_owned.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace performance_manager {
namespace mechanisms {

namespace {

using PolicyDelegate = TabLoadingFrameNavigationScheduler::PolicyDelegate;

// The default policy delegate that delegates directly to the
// TabLoadingFrameNavigationPolicy class.
class DefaultPolicyDelegate : public PolicyDelegate {
 public:
  using PolicyClass = policies::TabLoadingFrameNavigationPolicy;

  DefaultPolicyDelegate() = default;
  DefaultPolicyDelegate(const DefaultPolicyDelegate&) = delete;
  DefaultPolicyDelegate& operator=(const DefaultPolicyDelegate&) = delete;
  ~DefaultPolicyDelegate() override = default;

  // PolicyDelegate implementation:
  bool ShouldThrottleWebContents(content::WebContents* contents) override {
    return PolicyClass::ShouldThrottleWebContents(contents);
  }
  bool ShouldThrottleNavigation(content::NavigationHandle* handle) override {
    return PolicyClass::ShouldThrottleNavigation(handle);
  }

  static DefaultPolicyDelegate* Instance() {
    static base::NoDestructor<DefaultPolicyDelegate> default_policy_delegate;
    return default_policy_delegate.get();
  }
};

PolicyDelegate* g_policy_delegate = nullptr;
bool g_throttling_enabled = false;
TabLoadingFrameNavigationScheduler* g_root = nullptr;

PolicyDelegate* GetPolicyDelegate() {
  if (!g_policy_delegate)
    g_policy_delegate = DefaultPolicyDelegate::Instance();
  return g_policy_delegate;
}

class MainThreadMechanism : public PerformanceManagerMainThreadMechanism {
 public:
  MainThreadMechanism() = default;
  ~MainThreadMechanism() override = default;

  MainThreadMechanism(const MainThreadMechanism&) = delete;
  MainThreadMechanism& operator=(const MainThreadMechanism&) = delete;

  // PerformanceManagerMainThreadMechanism implementation:
  Throttles CreateThrottlesForNavigation(
      content::NavigationHandle* handle) override {
    auto throttle =
        TabLoadingFrameNavigationScheduler::MaybeCreateThrottleForNavigation(
            handle);
    Throttles throttles;
    if (throttle)
      throttles.push_back(std::move(throttle));
    return throttles;
  }

  static MainThreadMechanism* Instance() {
    // NOTE: We should really have the policy object create an instance of the
    // mechanism, and explicitly manage its lifetime, instead of having this
    // singleton proxy that lives forever. To do that properly we'll need
    // something like GraphRegistered for the main thread.
    static base::NoDestructor<MainThreadMechanism> instance;
    return instance.get();
  }
};

}  // namespace

// A very simple throttle that always defers until Resume is called. The
// scheduler that issued the throttle will always outlive all of the throttles
// themselves, or explicitly detach from the throttle.
class TabLoadingFrameNavigationScheduler::Throttle
    : public content::NavigationThrottle {
 public:
  Throttle(content::NavigationHandle* handle,
           TabLoadingFrameNavigationScheduler* scheduler)
      : content::NavigationThrottle(handle), scheduler_(scheduler) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "navigation", "TabLoadingFrameNavigationScheduler::Throttle", this,
        "url", handle->GetURL().spec());
    DCHECK(scheduler);
  }
  ~Throttle() override {
    if (scheduler_)
      scheduler_->NotifyThrottleDestroyed(this);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "navigation", "TabLoadingFrameNavigationScheduler::Throttle", this);
  }

  // content::NavigationThrottle implementation
  const char* GetNameForLogging() override {
    static constexpr char kName[] =
        "TabLoadingFrameNavigationScheduler::Throttle";
    return kName;
  }
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    return content::NavigationThrottle::DEFER;
  }

  // Make this public so the scheduler can invoke it. Care must be taken in
  // calling this as it can synchronously destroy this Throttle and others!
  using content::NavigationThrottle::Resume;

  // Detaches this Throttle from its scheduler, so it doesn't callback into it
  // when it is destroyed.
  void DetachFromScheduler() {
    // This should only be called once.
    DCHECK(scheduler_);
    scheduler_ = nullptr;
  }

 private:
  TabLoadingFrameNavigationScheduler* scheduler_ = nullptr;
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TabLoadingFrameNavigationScheduler)

TabLoadingFrameNavigationScheduler::~TabLoadingFrameNavigationScheduler() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(throttles_.empty());

  // Unlink ourselves from the linked list.
  if (g_root == this) {
    DCHECK_EQ(nullptr, prev_);
    g_root = next_;
  }
  if (prev_) {
    DCHECK_EQ(this, prev_->next_);
    prev_->next_ = next_;
    // Do not null |prev_| so we can access it below if needed.
  }
  if (next_) {
    DCHECK_EQ(this, next_->prev_);
    next_->prev_ = prev_;
    next_ = nullptr;
  }
  prev_ = nullptr;
}

// static
std::unique_ptr<content::NavigationThrottle>
TabLoadingFrameNavigationScheduler::MaybeCreateThrottleForNavigation(
    content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<content::NavigationThrottle> empty_throttle;

  if (!g_throttling_enabled)
    return empty_throttle;

  // Get the contents, and the associated scheduler if it exists.
  auto* contents = handle->GetWebContents();
  auto* scheduler = FromWebContents(contents);

  // If this is a non-main frame and no scheduler exists, then the decision was
  // already made to *not* throttle this contents, so we can return early.
  if (!handle->IsInMainFrame() && !scheduler) {
    return empty_throttle;
  }

  // If a scheduler exists and this is a main frame navigation then its a
  // renavigation and the contents is being reused. In this case we need to
  // tear down the existing scheduler, and potentially create a new one.
  if (handle->IsInMainFrame() && scheduler) {
    DCHECK_NE(handle->GetNavigationId(), scheduler->navigation_id_);
    scheduler->StopThrottlingImpl();  // Causes |scheduler| to delete itself.
    scheduler = FromWebContents(contents);
    DCHECK_EQ(nullptr, scheduler);
  }

  // If there's no scheduler for this contents, check the policy object to see
  // if one should be created.
  if (!scheduler) {
    DCHECK(handle->IsInMainFrame());
    if (!GetPolicyDelegate()->ShouldThrottleWebContents(contents)) {
      return empty_throttle;
    }
    CreateForWebContents(contents);
    scheduler = FromWebContents(contents);
    DCHECK(scheduler);
    scheduler->navigation_id_ = handle->GetNavigationId();
    // The main frame should never be throttled, so we can return early.
    return empty_throttle;
  }

  // At this point we have a scheduler, and the navigation is for a child
  // frame. Determine whether the child frame should be throttled.
  if (!GetPolicyDelegate()->ShouldThrottleNavigation(handle)) {
    return empty_throttle;
  }

  // Getting here indicates that the navigation is to be throttled. Create a
  // throttle and remember it.
  std::unique_ptr<Throttle> throttle(new Throttle(handle, scheduler));
  auto result =
      scheduler->throttles_.insert(std::make_pair(handle, throttle.get()));
  DCHECK(result.second);
  return throttle;
}

// static
void TabLoadingFrameNavigationScheduler::SetThrottlingEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (enabled == g_throttling_enabled)
    return;
  g_throttling_enabled = enabled;

  if (enabled) {
    PerformanceManager::AddMechanism(MainThreadMechanism::Instance());
    return;
  }

  // Remove the mechanism from the registry. Since the shutdown decision is made
  // on the PM sequence, this notification races with actual destruction of the
  // PM initiated on the UI sequence. As such, it's possible that the PM no
  // longer exists by the time we get here in a normal shutdown codepath.
  if (PerformanceManager::IsAvailable())
    PerformanceManager::RemoveMechanism(MainThreadMechanism::Instance());

  // At this point the throttling is being disabled. Stop throttling all
  // currently-throttled contents.
  while (g_root)
    g_root->StopThrottlingImpl();  // Causes |g_root| to delete itself.
}

// static
void TabLoadingFrameNavigationScheduler::StopThrottling(
    content::WebContents* contents,
    int64_t last_navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // This should never be called for a |contents| without an associated
  // scheduler, as besides contents re-use, all schedulers eventually receive
  // a StopThrottling notification.
  auto* scheduler = FromWebContents(contents);
  // There is a race between renavigations and policy messages. Only dispatch
  // this if the contents is still being throttled (the logic in
  // MaybeCreateThrottleForNavigation can cause the scheduler to be deleted),
  // and if its intended for the appropriate navigation ID.
  if (!scheduler || scheduler->navigation_id_ != last_navigation_id)
    return;
  scheduler->StopThrottlingImpl();
}

// static
void TabLoadingFrameNavigationScheduler::SetPolicyDelegateForTesting(
    PolicyDelegate* policy_delegate) {
  g_policy_delegate = policy_delegate;
}

// static
bool TabLoadingFrameNavigationScheduler::IsThrottlingEnabledForTesting() {
  return g_throttling_enabled;
}

// static
bool TabLoadingFrameNavigationScheduler::IsMechanismRegisteredForTesting() {
  return PerformanceManager::HasMechanism(MainThreadMechanism::Instance());
}

TabLoadingFrameNavigationScheduler::TabLoadingFrameNavigationScheduler(
    content::WebContents* contents)
    : content::WebContentsObserver(contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Link ourselves into the linked list.
  prev_ = nullptr;
  next_ = g_root;
  if (next_)
    next_->prev_ = this;
  g_root = this;
}

void TabLoadingFrameNavigationScheduler::DidFinishNavigation(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  RemoveThrottleForHandle(handle);
}

void TabLoadingFrameNavigationScheduler::StopThrottlingImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Release all of the throttles. Note that releasing a throttle may cause
  // that throttle and others to immediately be invalidated (we'll learn about
  // it via DidFinishNavigation() or NavigationThrottleDestroyed()).
  while (!throttles_.empty()) {
    // Remove the first throttle.
    auto it = throttles_.end() - 1;
    auto* throttle = it->second;
    throttles_.erase(it);

    // We've already erased this throttle and don't need it to notify us via
    // NotifyThrottleDestroyed.
    throttle->DetachFromScheduler();

    // Resume the throttle.
    if (resume_callback_)
      resume_callback_.Run(throttle);
    throttle->Resume();
  }

  // Tear down this object. This must be called last so as not to UAF ourselves.
  // Note that this is always called from static functions in this translation
  // unit, thus there are no other frames on the stack belonging to this object.
  web_contents()->RemoveUserData(UserDataKey());
}

void TabLoadingFrameNavigationScheduler::NotifyThrottleDestroyed(
    Throttle* throttle) {
  DCHECK(throttle);
  DCHECK(throttle->navigation_handle());
  RemoveThrottleForHandle(throttle->navigation_handle());
}

void TabLoadingFrameNavigationScheduler::RemoveThrottleForHandle(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = throttles_.find(handle);
  if (it == throttles_.end())
    return;
  // If we're throttling a canceled navigation then stop tracking it. The
  // |handle| becomes invalid shortly after this function returns. Explicitly
  // detach from the throttle so we don't get multiple callbacks from it.
  it->second->DetachFromScheduler();
  throttles_.erase(it);
}

}  // namespace mechanisms
}  // namespace performance_manager
