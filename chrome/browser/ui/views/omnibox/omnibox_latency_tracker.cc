// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_latency_tracker.h"

#include <array>
#include <utility>

#include "base/check_op.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace {

using Cuj = OmniboxLatencyTracker::Cuj;
using Event = OmniboxLatencyTracker::Event;
using Ui = OmniboxLatencyTracker::Ui;

bool g_startup_window_claimed = false;
std::optional<base::TimeTicks> g_application_start_for_testing;

// Returns true the first time it's called in this process. Called for the
// first show of every browser window, so that only the first window shown is
// the startup window.
bool ClaimStartupWindow() {
  return !std::exchange(g_startup_window_claimed, true);
}

// Returns the start of StartupToReady, the same as for the other startup
// metrics. It's null if there's none, e.g. on ChromeOS for a window that the
// user opened.
base::TimeTicks GetApplicationStart() {
  if (g_application_start_for_testing) {
    return *g_application_start_for_testing;
  }
  return startup_metric_utils::GetBrowser()
      .GetApplicationStartTicksForStartup();
}

// Whether other UI, e.g. the profile picker or a message box, interrupted the
// startup. The other startup metrics aren't recorded then either.
bool WasStartupInterrupted() {
  return !startup_metric_utils::GetBrowser().ShouldLogStartupHistogram();
}

// The histogram names, indexed by `Cuj` and `Ui`. They're static strings, since
// `EndingJourney` points to them.
constexpr std::array<std::array<const char*, 2>, 1> kHistogramNames = {{
    {"Omnibox.Latency.StartupToReady.Views",
     "Omnibox.Latency.StartupToReady.FullWebUI"},
}};

const char* GetHistogramName(Cuj cuj, Ui ui) {
  return kHistogramNames[std::to_underlying(cuj)][std::to_underlying(ui)];
}

// The journey names, indexed by `Cuj`, for trace events.
constexpr std::array<const char*, 1> kCujNames = {"StartupToReady"};

const char* GetCujName(Cuj cuj) {
  return kCujNames[std::to_underlying(cuj)];
}

// Marks in the trace that a journey was dropped without a sample.
void TraceJourneyDiscarded(Cuj cuj, const char* reason) {
  TRACE_EVENT_INSTANT("omnibox", "OmniboxLatencyTracker::JourneyDiscarded",
                      "cuj", GetCujName(cuj), "reason", reason);
}

// Whether `contents` auto-focuses the omnibox when it's shown. Only NTPs do,
// apart from the initial about:blank of new windows and popups.
bool AutoFocusesOmnibox(content::WebContents* contents) {
  content::WebContentsDelegate* delegate =
      contents ? contents->GetDelegate() : nullptr;
  return delegate && delegate->ShouldFocusLocationBarByDefault(contents);
}

}  // namespace

DEFINE_USER_DATA(OmniboxLatencyTracker);

OmniboxLatencyTracker::OmniboxLatencyTracker(ui::UnownedUserDataHost& host,
                                             const Delegate& delegate)
    : scoped_data_holder_(host, *this), delegate_(delegate) {}

OmniboxLatencyTracker::~OmniboxLatencyTracker() = default;

// static
OmniboxLatencyTracker* OmniboxLatencyTracker::From(
    BrowserWindowInterface* browser) {
  return browser ? Get(browser->GetUnownedUserDataHost()) : nullptr;
}

// static
void OmniboxLatencyTracker::Notify(BrowserWindowInterface* browser,
                                   Event event) {
  if (OmniboxLatencyTracker* tracker = From(browser)) {
    tracker->Notify(event);
  } else if (event == Event::kWindowShowing) {
    // A window without a tracker, e.g. a popup window, can be the startup
    // window too. No window measures StartupToReady then.
    ClaimStartupWindow();
  }
}

void OmniboxLatencyTracker::Notify(Event event) {
  switch (event) {
    case Event::kWindowShowing:
      OnWindowShowing();
      break;
    case Event::kWindowShownMinimized:
      // A window shown minimized only gets the focus once the user restores it.
      DiscardOngoingJourney("minimized");
      break;
    case Event::kWindowDeactivated:
      // The user switched away before the omnibox was ready. Ignored while the
      // omnibox can be auto-focused, since the Full WebUI omnibox popup can
      // take the activation from its window then.
      if (auto_focus_depth_ == 0) {
        DiscardOngoingJourney("window_deactivated");
      }
      break;
    case Event::kFocusRestoreStarted:
    case Event::kFullWebUIHandoffStarted:
      ++auto_focus_depth_;
      break;
    case Event::kFocusRestoreEnded:
      // The Views omnibox can hold the restored focus until the Full WebUI
      // omnibox is ready to take it over.
      EndAutoFocus(
          /*keep_journey=*/delegate_->IsAwaitingFullWebUIFocusHandoff());
      break;
    case Event::kFullWebUIHandoffEnded:
      EndAutoFocus(/*keep_journey=*/false);
      break;
    case Event::kFullWebUIReadyWithoutFocus:
      // The Views omnibox lost the focus that it held for the handoff, unless
      // the window's first activation is yet to restore the focus.
      if (delegate_->IsWindowActive()) {
        DiscardOngoingJourney("no_focus_handoff");
      }
      break;
    case Event::kUserFocusedOmnibox:
      // The omnibox wasn't auto-focused first.
      DiscardOngoingJourney("user_focus");
      break;
  }
}

// static
std::optional<OmniboxLatencyTracker::EndingJourney>
OmniboxLatencyTracker::NotifyOmniboxFocused(
    BrowserWindowInterface* browser,
    Ui ui,
    content::WebContents* active_contents) {
  OmniboxLatencyTracker* tracker = From(browser);
  return tracker ? tracker->NotifyOmniboxFocused(ui, active_contents)
                 : std::nullopt;
}

std::optional<OmniboxLatencyTracker::EndingJourney>
OmniboxLatencyTracker::NotifyOmniboxFocused(
    Ui ui,
    content::WebContents* active_contents) {
  if (!ongoing_journey_) {
    return std::nullopt;
  }
  // Any other focus doesn't come from the startup, e.g. the user focused the
  // omnibox with a tap or the Tab key.
  if (auto_focus_depth_ == 0) {
    DiscardOngoingJourney("focus_outside_auto_focus");
    return std::nullopt;
  }
  if (!AutoFocusesOmnibox(active_contents)) {
    return std::nullopt;
  }
  const OngoingJourney journey = *std::exchange(ongoing_journey_, std::nullopt);
  // The other startup metrics check this when they record too, since other UI,
  // e.g. a message box, can interrupt the startup after the window shows.
  if (journey.cuj == Cuj::kStartupToReady && WasStartupInterrupted()) {
    TraceJourneyDiscarded(journey.cuj, "startup_interrupted");
    return std::nullopt;
  }
  const char* histogram_name = GetHistogramName(journey.cuj, ui);
  TRACE_EVENT_INSTANT("omnibox", "OmniboxLatencyTracker::JourneyEnded", "cuj",
                      GetCujName(journey.cuj), "histogram", histogram_name);
  return EndingJourney{.histogram_name = histogram_name,
                       .start_time = journey.start_time};
}

void OmniboxLatencyTracker::SetOngoingJourneyForTesting(
    std::optional<base::TimeTicks> start_time) {
  ongoing_journey_.reset();
  if (start_time) {
    ongoing_journey_ =
        OngoingJourney{.cuj = Cuj::kStartupToReady, .start_time = *start_time};
  }
}

// static
void OmniboxLatencyTracker::ResetStartupWindowForTesting() {
  g_startup_window_claimed = false;
}

// static
base::AutoReset<std::optional<base::TimeTicks>>
OmniboxLatencyTracker::SetApplicationStartForTesting(
    base::TimeTicks application_start) {
  return base::AutoReset<std::optional<base::TimeTicks>>(
      &g_application_start_for_testing, application_start);
}

void OmniboxLatencyTracker::OnWindowShowing() {
  if (std::exchange(window_shown_, true)) {
    return;
  }
  // The first browser window shown in this process is the startup window.
  if (!ClaimStartupWindow()) {
    return;
  }
  // Like the other startup metrics, skip startups that other UI, e.g. the
  // profile picker, interrupted.
  const base::TimeTicks application_start = GetApplicationStart();
  if (application_start.is_null() || WasStartupInterrupted()) {
    return;
  }
  TRACE_EVENT_INSTANT(
      "omnibox", "OmniboxLatencyTracker::JourneyStarted", "cuj",
      GetCujName(Cuj::kStartupToReady), "start_to_now_ms",
      (base::TimeTicks::Now() - application_start).InMillisecondsF());
  ongoing_journey_ = OngoingJourney{.cuj = Cuj::kStartupToReady,
                                    .start_time = application_start};
}

void OmniboxLatencyTracker::EndAutoFocus(bool keep_journey) {
  CHECK_GT(auto_focus_depth_, 0);
  if (--auto_focus_depth_ == 0 && !keep_journey) {
    DiscardOngoingJourney("not_auto_focused");
  }
}

void OmniboxLatencyTracker::DiscardOngoingJourney(const char* reason) {
  if (ongoing_journey_) {
    TraceJourneyDiscarded(ongoing_journey_->cuj, reason);
    ongoing_journey_.reset();
  }
}
