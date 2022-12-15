// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/actions/omnibox_action.h"

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "build/build_config.h"
#include "components/omnibox/browser/omnibox_client.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#endif

OmniboxAction::LabelStrings::LabelStrings(int id_hint,
                                          int id_suggestion_contents,
                                          int id_accessibility_suffix,
                                          int id_accessibility_hint)
    : hint(l10n_util::GetStringUTF16(id_hint)),
      suggestion_contents(l10n_util::GetStringUTF16(id_suggestion_contents)),
      accessibility_suffix(l10n_util::GetStringUTF16(id_accessibility_suffix)),
      accessibility_hint(l10n_util::GetStringUTF16(id_accessibility_hint)) {}

OmniboxAction::LabelStrings::LabelStrings() = default;

OmniboxAction::LabelStrings::LabelStrings(const LabelStrings&) = default;

OmniboxAction::LabelStrings::~LabelStrings() = default;

namespace base {
namespace trace_event {
size_t EstimateMemoryUsage(const OmniboxAction::LabelStrings& self) {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(self.hint);
  total += base::trace_event::EstimateMemoryUsage(self.suggestion_contents);
  total += base::trace_event::EstimateMemoryUsage(self.accessibility_suffix);
  total += base::trace_event::EstimateMemoryUsage(self.accessibility_hint);
  return total;
}
}  // namespace trace_event
}  // namespace base

// =============================================================================

bool OmniboxAction::Client::OpenJourneys(const std::string& query) {
  return false;
}

// =============================================================================

OmniboxAction::ExecutionContext::ExecutionContext(
    Client& client,
    OpenUrlCallback callback,
    base::TimeTicks match_selection_timestamp,
    WindowOpenDisposition disposition)
    : client_(client),
      open_url_callback_(std::move(callback)),
      match_selection_timestamp_(match_selection_timestamp),
      disposition_(disposition) {}

OmniboxAction::ExecutionContext::~ExecutionContext() = default;

// =============================================================================

OmniboxAction::OmniboxAction(LabelStrings strings,
                             GURL url,
                             bool takes_over_match)
    : strings_(strings), url_(url), takes_over_match_(takes_over_match) {}

OmniboxAction::~OmniboxAction() = default;

const OmniboxAction::LabelStrings& OmniboxAction::GetLabelStrings() const {
  return strings_;
}

void OmniboxAction::Execute(OmniboxAction::ExecutionContext& context) const {
  DCHECK(url_.is_valid());
  OpenURL(context, url_);
}

bool OmniboxAction::IsReadyToTrigger(
    const AutocompleteInput& input,
    const AutocompleteProviderClient& client) const {
  return true;
}

bool OmniboxAction::TakesOverMatch() const {
  return takes_over_match_;
}

#if defined(SUPPORT_PEDALS_VECTOR_ICONS)
const gfx::VectorIcon& OmniboxAction::GetVectorIcon() const {
  // TODO(tommycli): Replace with real icon.
  return omnibox::kPedalIcon;
}
#endif

size_t OmniboxAction::EstimateMemoryUsage() const {
  size_t total = 0;
  total += base::trace_event::EstimateMemoryUsage(url_);
  total += base::trace_event::EstimateMemoryUsage(strings_);
  return total;
}

int32_t OmniboxAction::GetID() const {
  return 0;
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaGlobalRef<jobject> OmniboxAction::GetJavaObject()
    const {
  return base::android::ScopedJavaGlobalRef<jobject>();
}
#endif

void OmniboxAction::OpenURL(OmniboxAction::ExecutionContext& context,
                            const GURL& url) const {
  // Set `match_type` as if the user just typed |url| verbatim.
  // `destination_url_entered_without_scheme` is used to determine whether
  // navigations typed without a scheme and upgraded to HTTPS should fall back
  // to HTTP. The URL might have been entered without a scheme, but Action
  // destination URLs don't need a fallback so it's fine to pass false here.
  std::move(context.open_url_callback_)
      .Run(url, nullptr, context.disposition_, ui::PAGE_TRANSITION_GENERATED,
           /*match_type=*/AutocompleteMatchType::URL_WHAT_YOU_TYPED,
           context.match_selection_timestamp_,
           /*destination_url_entered_without_scheme=*/false, u"",
           AutocompleteMatch(), AutocompleteMatch(),
           IDNA2008DeviationCharacter::kNone);
}
