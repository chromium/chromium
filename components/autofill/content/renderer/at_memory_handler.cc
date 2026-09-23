// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/at_memory_handler.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/hash/hash.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/autofill/content/renderer/autofill_agent.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/synchronous_form_cache.h"
#include "components/autofill/content/renderer/timing.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/field_data_manager.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_input_method_controller.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace autofill {

namespace {

using ::blink::RendererPreferences;
using ::blink::WebElement;
using ::blink::WebFormControlElement;
using ::blink::WebFormElement;
using ::blink::WebKeyboardEvent;
using ::blink::WebLocalFrame;
using ::blink::WebNode;
using ::blink::WebRange;
using ::blink::WebString;

// If more time than this happens between two keystrokes, they're not considered
// as belonging to the same coherent input (e.g., double Ctrl).
constexpr base::TimeDelta kCoherentKeyDownThreshold = base::Milliseconds(500);

// Returns true if `event` may produce a character.
bool IsPrintable(const WebKeyboardEvent& event) {
  if (base::IsAsciiControl(event.text[0]) || event.text[1] != 0) {
    return false;
  }
  if constexpr (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)) {
    // On Linux and Windows, Alt+X is not printable.
    return !(event.GetModifiers() & blink::WebInputEvent::kAltKey);
  }
  if constexpr (BUILDFLAG(IS_MAC)) {
    // On Mac, Meta+X is not printable but leads to `event.text[0] != 'X'`.
    return !(event.GetModifiers() & blink::WebInputEvent::kMetaKey);
  }
  return true;
}

bool IsSingleCtrlKey(const WebKeyboardEvent& event) {
  switch (event.windows_key_code) {
#if BUILDFLAG(IS_MAC)
    // On Mac, we use Command instead of Ctrl.
    case ui::VKEY_COMMAND:
    case ui::VKEY_RIGHT_COMMAND:
      return (event.GetModifiers() & WebKeyboardEvent::kKeyModifiers) ==
             WebKeyboardEvent::kMetaKey;
#else
    case ui::VKEY_CONTROL:
    case ui::VKEY_LCONTROL:
    case ui::VKEY_RCONTROL:
      return (event.GetModifiers() & WebKeyboardEvent::kKeyModifiers) ==
             WebKeyboardEvent::kControlKey;
#endif
    default:
      return false;
  }
}

// Returns true if window-level focus is in the web content area, or more
// precisely, in one of the frames of this renderer process.
bool HasWindowFocus(const blink::WebLocalFrame& frame) {
  blink::WebFrameWidget* widget = frame.LocalRoot()->FrameWidget();
  return widget && widget->HasFocus();
}

// Returns true if `field` is fillable by AtMemory.
bool IsSupportedField(const WebElement& field) {
  if (!field || !form_util::IsAccessible(field)) {
    return false;
  }
  if (const auto form_control = field.DynamicTo<WebFormControlElement>()) {
    return form_util::IsTextAreaElementOrTextInput(form_control) &&
           form_util::GetAutofillFormControlType(form_control) !=
               FormControlType::kInputPassword &&
           form_control.IsEnabled() && !form_control.IsReadOnly();
  }
  return field.IsContentEditable() && !field.DynamicTo<WebFormElement>();
}

size_t HashFieldValue(const WebElement& field) {
  const WebString value = [&] {
    if (auto form_control = field.DynamicTo<WebFormControlElement>()) {
      return form_control.Value();
    }
    return field.TextContent();
  }();
  return base::FastHash(base::as_byte_span(value.Utf16()));
}

}  // namespace

AtMemoryHandler::AtMemoryHandler(AutofillAgent* agent)
    : agent_(CHECK_DEREF(agent)) {}

AtMemoryHandler::~AtMemoryHandler() = default;

bool AtMemoryHandler::DidReceiveKeyDown(const WebElement& field,
                                        const WebKeyboardEvent& event) {
  // Runs before the `kAutofillAtMemory` check: the Autofill.DoubleCtrlPressed
  // UKM event is recorded for all users, whether or not AtMemory is enabled.
  DidReceiveKeyDownForDoubleCtrl(field, event);

  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory)) {
    return false;
  }
  return DidReceiveKeyDownForTriggerShortcut(field, event);
}

bool AtMemoryHandler::DidReceiveKeyDownForTriggerShortcut(
    const WebElement& field,
    const WebKeyboardEvent& event) {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAtMemoryTriggerShortcut)) {
    return false;
  }

  const RendererPreferences* prefs = GetRendererPreferences();
  if (!prefs || prefs->autofill_shortcut_key_code == ui::VKEY_UNKNOWN) {
    return false;
  }

  // The configured keyboard shortcut opens the Autofill AtMemory popup.
  const ui::Accelerator expected_accelerator(
      prefs->autofill_shortcut_key_code, prefs->autofill_shortcut_modifiers);
  const ui::Accelerator actual_accelerator(
      static_cast<ui::KeyboardCode>(event.windows_key_code),
      ui::WebEventModifiersToEventFlags(event.GetModifiers()));
  if (expected_accelerator != actual_accelerator || IsPrintable(event)) {
    return false;
  }

  if (auto form_control = field.DynamicTo<WebFormControlElement>();
      form_control && form_util::IsTextAreaElementOrTextInput(form_control) &&
      form_control.FormControlTypeForAutofill() !=
          blink::mojom::FormControlType::kInputPassword &&
      form_control.IsEnabled() && !form_control.IsReadOnly()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestions(
          form_control,
          AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut,
          SynchronousFormCache(), std::nullopt);
    }
    return true;  // Prevent default.
  } else if (field.IsContentEditable()) {
    if (!actual_accelerator.IsRepeat()) {
      agent_->ShowSuggestionsForContentEditable(
          field, AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut);
    }
    return true;  // Prevent default.
  }
  return false;
}

void AtMemoryHandler::DidReceiveKeyDownForDoubleCtrl(
    const WebElement& field,
    const WebKeyboardEvent& event) {
  if (!IsSingleCtrlKey(event) ||
      (event.GetModifiers() & blink::WebInputEvent::kIsAutoRepeat)) {
    ctrl_state_ = {};
    return;
  }

  if (!IsSupportedField(field) || !field.ContainsFrameSelection()) {
    ctrl_state_ = {};
    return;
  }

  const FieldRendererId field_id = form_util::GetFieldRendererId(field);
  const base::TimeTicks now = base::TimeTicks::Now();

  if (ctrl_state_.last_ctrl_dom_code != event.dom_code ||
      ctrl_state_.last_field_id != field_id ||
      now - ctrl_state_.last_time > kCoherentKeyDownThreshold) {
    ctrl_state_ = {.last_ctrl_dom_code = event.dom_code,
                   .last_time = now,
                   .last_field_id = field_id};
    // The double Ctrl sequence isn't complete yet.
    return;
  }

  // The double Ctrl sequence is complete.
  ctrl_state_ = {};

  RecordDoubleCtrl(field);

  // The UKM event above is unconditional; showing suggestions is not.
  if (!IsDoubleCtrlTriggerEnabled()) {
    return;
  }

  if (auto form_control = field.DynamicTo<WebFormControlElement>()) {
    agent_->ShowSuggestions(
        form_control, AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl,
        SynchronousFormCache(), std::nullopt);
  } else {
    DCHECK(field.IsContentEditable());
    agent_->ShowSuggestionsForContentEditable(
        field, AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl);
  }
}

void AtMemoryHandler::FocusedElementChanged(
    const WebElement& new_focused_element) {
  ctrl_state_ = {};
}

void AtMemoryHandler::DidReceiveLeftMouseDownOrGestureTapInNode(
    const blink::WebNode& node) {
  ctrl_state_ = {};
}

void AtMemoryHandler::ReplaceSelectionForAtMemory(WebElement field,
                                                  const std::u16string& value) {
  const std::optional<AskForValuesToFillInfo> info =
      ExtractAskForValuesToFill(field);
  if (!info) {
    return;
  }
  WaitForFocusAndReplaceSelectionForAtMemory(*std::move(info), value,
                                             /*num_try=*/0);
}

void AtMemoryHandler::WaitForFocusAndReplaceSelectionForAtMemory(
    AskForValuesToFillInfo info,
    std::u16string value,
    int num_try) {
  constexpr int kMaxRetries = 5;
  constexpr base::TimeDelta kDelayBeforeRetry = base::Milliseconds(20);

  auto get_frame = [field_id = info.field_id]() -> WebLocalFrame* {
    WebElement field =
        WebNode::FromDomNodeId(*field_id).DynamicTo<WebElement>();
    return field ? field.GetDocument().GetFrame() : nullptr;
  };

  WebLocalFrame* frame = get_frame();
  if (!frame) {
    return;
  }

  // Wait for window-level focus to be returned by the browser process to the
  // web contents.
  //
  // The browser process steals the window-level focus to display the AtMemory
  // popup (which contains a text field) and returns it when the popup is
  // closed. We wait for the focus `kDelayBeforeRetry * kMaxRetries`.
  if (!HasWindowFocus(*frame) && num_try < kMaxRetries) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &AtMemoryHandler::WaitForFocusAndReplaceSelectionForAtMemory,
            weak_ptr_factory_.GetWeakPtr(), std::move(info), std::move(value),
            num_try + 1),
        kDelayBeforeRetry);
    return;
  }

  WebElement field =
      WebNode::FromDomNodeId(*info.field_id).DynamicTo<WebElement>();
  if (!field) {
    return;
  }

  // Abort filling if the field value changed in the meantime because we
  // shouldn't overwrite other content. This is particularly relevant when
  // AtMemory has a high filling latency due to SPII fetching.
  if (info.value_hash != HashFieldValue(field)) {
    return;
  }

  // Ensures that `field.GetDocument().FocusedElement() == field` and that
  // SetEditableSelectionOffsets() and ExtendSelectionAndReplace() operate on
  // that field (assuming no JavaScript `focus` listener moves it elsewhere).
  //
  // This is to handle the case where DOM-level focus moved out of the field.
  // That may happen especially when AtMemory has a high filling latency due to
  // SPII fetching.
  //
  // We do this intentionally after the hash value comparison so we don't move
  // the focus if no fill happens.
  field.Focus();

  if (!info.selection_range.IsNull()) {
    // `field.Focus()` dispatches `blur`/`focusout` on the previously focused
    // element and `focus`/`focusin` on `field`. Those listeners may run
    // JavaScript that detaches the frame, which would leave `frame` dangling.
    // Therefore, re-fetch it before using it below.
    frame = get_frame();
    if (!frame) {
      return;
    }
    // Restores the text selection at the time of AskForValuesToFill() so that
    // filling replaces the selected text.
    frame->SetEditableSelectionOffsets(info.selection_range.StartOffset(),
                                       info.selection_range.EndOffset());
  }

  if (base::FeatureList::IsEnabled(features::kAutofillAtMemoryPasteText)) {
    field.PasteText(WebString::FromUtf16(value), /*replace_all=*/false,
                    /*smart_replace=*/true);
  } else {
    frame = get_frame();
    if (!frame) {
      return;
    }
    frame->ExtendSelectionAndReplace(/*before=*/0,
                                     /*after=*/0, WebString::FromUtf16(value));
  }
}

std::optional<AtMemoryHandler::AskForValuesToFillInfo>
AtMemoryHandler::ExtractAskForValuesToFill(const WebElement& field) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!field.DynamicTo<WebFormElement>());
  auto it = std::ranges::find(last_at_memory_ask_for_values_to_fills_,
                              form_util::GetFieldRendererId(field),
                              &AskForValuesToFillInfo::field_id);
  if (it == last_at_memory_ask_for_values_to_fills_.end()) {
    return std::nullopt;
  }
  AskForValuesToFillInfo info = *it;
  last_at_memory_ask_for_values_to_fills_.erase(it);
  return info;
}

void AtMemoryHandler::MaybeUpdateAskForValuesToFill(
    const WebElement& field,
    AutofillSuggestionTriggerSource trigger_source) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!field.DynamicTo<WebFormElement>());
  if (!IsAtMemoryTriggerSource(trigger_source)) {
    return;
  }

  ExtractAskForValuesToFill(field);

  constexpr size_t kMaxSize = 10;
  while (last_at_memory_ask_for_values_to_fills_.size() >= kMaxSize) {
    last_at_memory_ask_for_values_to_fills_.pop_front();
  }

  WebLocalFrame* frame = field.GetDocument().GetFrame();

  last_at_memory_ask_for_values_to_fills_.push_back(AskForValuesToFillInfo{
      .field_id = form_util::GetFieldRendererId(field),
      .value_hash = HashFieldValue(field),
      .selection_range =
          frame ? frame->GetInputMethodController()->GetSelectionOffsets()
                : WebRange()});
}

ukm::UkmRecorder* AtMemoryHandler::GetUkmRecorder() {
  if (!ukm_recorder_) {
    mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
    content::RenderThread::Get()->BindHostReceiver(
        factory.BindNewPipeAndPassReceiver());
    ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
  }
  return ukm_recorder_.get();
}

bool AtMemoryHandler::IsDoubleCtrlTriggerEnabled() const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAtMemory) ||
      !base::FeatureList::IsEnabled(features::kAutofillAtMemoryDoubleCtrl)) {
    return false;
  }
  const RendererPreferences* prefs = GetRendererPreferences();
  return prefs && prefs->autofill_at_memory_double_ctrl_trigger_enabled;
}

void AtMemoryHandler::RecordDoubleCtrl(const WebElement& field) {
  // This function is intended only for WebFormControlElements and for
  // contenteditables that aren't WebFormElement. See
  // form_util::GetFieldRendererId().
  CHECK(!field.DynamicTo<WebFormElement>());
  const ukm::SourceId source_id = field.GetDocument()
                                      ? field.GetDocument().GetUkmSourceId()
                                      : ukm::kInvalidSourceId;
  ukm::UkmRecorder* recorder = GetUkmRecorder();
  if (!recorder || source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::Autofill_DoubleCtrlPressed builder(source_id);
  builder.SetAtMemoryTriggerRequested(IsDoubleCtrlTriggerEnabled());

  auto set_metrics = [&](const FormData& form_data,
                         const FormFieldData& field_data) {
    builder.SetFormSignature(
        HashFormSignature(CalculateFormSignature(form_data)));
    builder.SetFieldSignature(
        HashFieldSignature(CalculateFieldSignatureForField(field_data)));
    builder.SetFormControlType(
        std::to_underlying(field_data.form_control_type()));
    if (WebLocalFrame* frame = field.GetDocument().GetFrame()) {
      const FieldRendererId field_id = field_data.renderer_id();
      const blink::LocalFrameToken frame_token = frame->GetLocalFrameToken();
      builder.SetFieldSessionIdentifier(StrToHash64Bit(
          base::NumberToString(field_id.value()) + frame_token.ToString()));
    }
  };

  if (WebFormControlElement form_control =
          field.DynamicTo<WebFormControlElement>()) {
    if (std::optional<form_util::FormAndField> form_and_field =
            form_util::FindFormAndFieldForFormControlElement(
                form_control, agent_->field_data_manager(),
                agent_->GetCallTimerState(
                    CallTimerState::CallSite::kDidReceiveKeyDown),
                agent_->button_titles_cache(), /*form_cache=*/{})) {
      set_metrics(form_and_field->form, form_and_field->field);
    }
  } else {
    DCHECK(field.IsContentEditable());
    if (std::optional<FormData> form_data =
            form_util::FindFormForContentEditable(field)) {
      if (!form_data->fields().empty()) {
        set_metrics(*form_data, form_data->fields().front());
      }
    }
  }

  builder.Record(recorder);
}

const RendererPreferences* AtMemoryHandler::GetRendererPreferences() const {
  if (auto* frame = agent_->unsafe_render_frame()) {
    if (auto* web_frame = frame->GetWebFrame()) {
      if (auto* view = web_frame->View()) {
        return &view->GetRendererPreferences();
      }
    }
  }
  return nullptr;
}

}  // namespace autofill
