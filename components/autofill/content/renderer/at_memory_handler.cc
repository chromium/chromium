// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/at_memory_handler.h"

#include <algorithm>
#include <optional>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/hash/hash.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/autofill_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"

namespace autofill {

namespace {

using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFormElement;
using ::blink::WebString;

}  // namespace

AtMemoryHandler::AtMemoryHandler() = default;

AtMemoryHandler::~AtMemoryHandler() = default;

std::optional<AtMemoryHandler::AskForValuesToFillInfo>
AtMemoryHandler::FindAskForValuesToFill(const WebElement& element, bool pop) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!element.DynamicTo<WebFormElement>());
  auto it = std::ranges::find(last_at_memory_ask_for_values_to_fills_,
                              form_util::GetFieldRendererId(element),
                              &AskForValuesToFillInfo::field_id);
  if (it == last_at_memory_ask_for_values_to_fills_.end()) {
    return std::nullopt;
  }
  AskForValuesToFillInfo info = *it;
  if (pop) {
    last_at_memory_ask_for_values_to_fills_.erase(it);
  }

  WebString value = [&] {
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return element.TextContent();
  }();
  if (info.value_hash != base::FastHash(base::as_byte_span(value.Utf16()))) {
    return std::nullopt;
  }

  return info;
}

void AtMemoryHandler::MaybeUpdateAskForValuesToFill(
    const WebElement& element,
    AutofillSuggestionTriggerSource trigger_source) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!element.DynamicTo<WebFormElement>());
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  FindAskForValuesToFill(element, /*pop=*/true);

  static constexpr size_t kMaxSize = 10;
  while (last_at_memory_ask_for_values_to_fills_.size() >= kMaxSize) {
    last_at_memory_ask_for_values_to_fills_.pop_front();
  }

  WebString value = [&] {
    if (auto form_control = element.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return element.TextContent();
  }();

  last_at_memory_ask_for_values_to_fills_.push_back(AskForValuesToFillInfo{
      .field_id = form_util::GetFieldRendererId(element),
      .caused_by_trigger_string =
          trigger_source ==
          AutofillSuggestionTriggerSource::kAtMemoryTriggerString,
      .value_hash = base::FastHash(base::as_byte_span(value.Utf16()))});
}

}  // namespace autofill
