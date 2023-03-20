// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/content_autofill_router.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/invoke.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy_features.h"

// AFCHECK(condition[, error_handler]) creates a crash dump and executes
// |error_handler| if |condition| is false.
// TODO(crbug/1187842): Replace AFCHECK() with DCHECK().
#define AFCHECK(condition, ...)                                                \
  if (!(condition)) {                                                          \
    SCOPED_CRASH_KEY_STRING256("autofill", "main_url", MainUrlForDebugging()); \
    AFCRASHDUMP();                                                             \
    __VA_ARGS__;                                                               \
  }
#if DCHECK_IS_ON()
#define AFCRASHDUMP() DCHECK(false)
#else
#define AFCRASHDUMP() base::debug::DumpWithoutCrashing()
#endif

namespace autofill {

namespace {

// Calls |fun| for all drivers in |form_forest|.
template <typename UnaryFunction>
void ForEachFrame(internal::FormForest& form_forest, UnaryFunction fun) {
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillAcrossIframes));
  for (const std::unique_ptr<internal::FormForest::FrameData>& some_frame :
       form_forest.frame_datas()) {
    // Required for AFCHECK().
    auto MainUrlForDebugging = []() { return std::string(); };
    AFCHECK(some_frame, continue);
    if (some_frame->driver)
      base::invoke(fun, some_frame->driver);
  }
}

}  // namespace

ContentAutofillRouter::ContentAutofillRouter() = default;
ContentAutofillRouter::~ContentAutofillRouter() = default;

std::string ContentAutofillRouter::MainUrlForDebugging() const {
  content::RenderFrameHost* some_rfh =
      content::RenderFrameHost::FromID(some_rfh_for_debugging_);
  if (!some_rfh) {
    for (const auto& frame_data : form_forest_.frame_datas()) {
      if (frame_data && frame_data->driver)
        some_rfh = frame_data->driver->render_frame_host();
    }
  }
  if (!some_rfh)
    return std::string();
  return some_rfh->GetMainFrame()->GetLastCommittedURL().spec();
}

ContentAutofillDriver* ContentAutofillRouter::DriverOfFrame(
    LocalFrameToken frame) {
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillAcrossIframes));
  const auto& frames = form_forest_.frame_datas();
  auto it = frames.find(frame);
  return it != frames.end() ? (*it)->driver.get() : nullptr;
}

void ContentAutofillRouter::UnregisterDriver(ContentAutofillDriver* driver) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes))
    return;

  some_rfh_for_debugging_ = content::GlobalRenderFrameHostId();

  AFCHECK(driver, return );

  for (const std::unique_ptr<internal::FormForest::FrameData>& frame :
       form_forest_.frame_datas()) {
    AFCHECK(frame, continue);
    if (frame->driver == driver) {
      form_forest_.EraseFrame(frame->frame_token);
      break;
    }
  }

  if (last_queried_source_ == driver)
    SetLastQueriedSource(nullptr);
  if (last_queried_target_ == driver)
    SetLastQueriedTarget(nullptr);
}

void ContentAutofillRouter::SetLastQueriedSource(
    ContentAutofillDriver* source) {
  if (last_queried_source_ && last_queried_source_ != source) {
    last_queried_source_->UnsetKeyPressHandlerCallback();
    last_queried_source_->SetShouldSuppressKeyboardCallback(false);
  }
  last_queried_source_ = source;
}

void ContentAutofillRouter::SetLastQueriedTarget(
    ContentAutofillDriver* target) {
  last_queried_target_ = target;
}

void ContentAutofillRouter::SetKeyPressHandler(
    ContentAutofillDriver* source,
    const content::RenderWidgetHost::KeyPressEventCallback& handler,
    void (*callback)(
        ContentAutofillDriver* target,
        const content::RenderWidgetHost::KeyPressEventCallback& handler)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, handler);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // The asynchronous AutocompleteHistoryManager::OnAutofillValuesReturned()
  // calls SetKeyPressHandler() through AutofillPopupControllerImpl::Show().
  // Before this call, UnregisterDriver() may have reset |last_queried_source_|
  // already to nullptr due to a race condition with AutocompleteHistoryManager
  // (https://crbug.com/1254173).
  if (!last_queried_source_)
    return;

  callback(last_queried_source_, handler);
}

void ContentAutofillRouter::UnsetKeyPressHandler(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // When AutofillPopupControllerImpl::Hide() calls this function,
  // UnregisterDriver() may have reset |last_queried_source_| already to
  // nullptr due to Mojo race conditions (https://crbug.com/1240246).
  if (!last_queried_source_)
    return;

  callback(last_queried_source_);
}

void ContentAutofillRouter::SetShouldSuppressKeyboard(
    ContentAutofillDriver* source,
    bool suppress,
    void (*callback)(ContentAutofillDriver* target, bool suppress)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, suppress);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // TODO(crbug.com/1247698): Double check if this could happen.
  if (!last_queried_source_)
    return;

  callback(last_queried_source_, suppress);
}

// Routing of events called by the renderer:

// Calls TriggerReparse() on all ContentAutofillDrivers in |form_forest_| as
// well as their ancestor ContentAutofillDrivers.
//
// An ancestor might not be contained in the form tree itself: if the ancestor
// contained only invisible iframe(s) and no interesting fields, it would not be
// sent to the browser. In the meantime, these frames may have become visible.
//
// The typical use case is that some frame triggers reparses on its own
// initiative and triggers an event. Then ContentAutofillRouter's event handler
// tells the other frames to reparse, too, using TriggerReparseExcept(source).
void ContentAutofillRouter::TriggerReparseExcept(
    ContentAutofillDriver* exception) {
  DCHECK(base::FeatureList::IsEnabled(features::kAutofillAcrossIframes));

  base::flat_set<ContentAutofillDriver*> already_triggered;
  ForEachFrame(form_forest_, [&](ContentAutofillDriver* driver) mutable {
    content::RenderFrameHost* rfh = driver->render_frame_host();
    do {
      // Trigger reparse for |rfh| and all its ancestors (as some
      // ancestors may not be in the forest).
      ContentAutofillDriver* rfh_driver =
          ContentAutofillDriver::GetForRenderFrameHost(rfh);
      AFCHECK(rfh_driver, continue);
      if (rfh_driver != exception &&
          !base::Contains(already_triggered, rfh_driver)) {
        rfh_driver->TriggerReparse();
        already_triggered.insert(rfh_driver);
      }
    } while ((rfh = rfh->GetParent()) != nullptr);
  });
}

void ContentAutofillRouter::FormsSeen(
    ContentAutofillDriver* source,
    std::vector<FormData> renderer_forms,
    const std::vector<FormGlobalId>& removed_forms,
    void (*callback)(ContentAutofillDriver* target,
                     const std::vector<FormData>& updated_forms,
                     const std::vector<FormGlobalId>& removed_forms)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, renderer_forms, removed_forms);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  base::flat_set<FormGlobalId> forms_with_removed_fields =
      form_forest_.EraseForms(removed_forms);

  std::vector<FormGlobalId> renderer_form_ids;
  renderer_form_ids.reserve(renderer_forms.size());
  for (const FormData& renderer_form : renderer_forms) {
    renderer_form_ids.push_back(renderer_form.global_id());
  }

  for (FormData& form : std::move(renderer_forms)) {
    form_forest_.UpdateTreeOfRendererForm(std::move(form), source);
  }

  // Collects the browser forms of the |renderer_forms_ids|. If all forms in
  // |renderer_forms_ids| are root forms, each of them has a different browser
  // form. Otherwise, all forms in |renderer_forms_ids| are non-root forms in
  // the same tree, and |browser_forms| will contain the flattened root of this
  // tree.
  std::vector<FormData> browser_forms;
  browser_forms.reserve(renderer_form_ids.size());
  for (FormGlobalId renderer_form_id : renderer_form_ids) {
    const FormData* browser_form =
        form_forest_.GetBrowserForm(renderer_form_id);
    AFCHECK(browser_form, return);
    if (!base::Contains(browser_forms, browser_form->global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(*browser_form);
    }
  }
  DCHECK(browser_forms.size() == renderer_form_ids.size() ||
         browser_forms.size() == 1);

  for (const FormGlobalId form_id : forms_with_removed_fields) {
    const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
    AFCHECK(browser_form, continue);
    if (!base::Contains(browser_forms, browser_form->global_id(),
                        &FormData::global_id)) {
      browser_forms.push_back(*browser_form);
    }
  }

  // Send the browser forms to the individual frames.
  if (!browser_forms.empty()) {
    LocalFrameToken frame = browser_forms.front().host_frame;
    DCHECK(base::ranges::all_of(browser_forms, [frame](const FormData& f) {
      return f.host_frame == frame;
    }));
    ContentAutofillDriver* target = DriverOfFrame(frame);
    AFCHECK(target, return );
    callback(target, browser_forms, removed_forms);
  } else if (!removed_forms.empty()) {
    callback(source, {}, removed_forms);
  }
}

void ContentAutofillRouter::SetFormToBeProbablySubmitted(
    ContentAutofillDriver* source,
    absl::optional<FormData> form,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData* optional_form)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, form ? &*form : nullptr);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (!form) {
    callback(source, nullptr);
    return;
  }

  FormGlobalId form_id = form->global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form).value(), source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, browser_form);
}

void ContentAutofillRouter::FormSubmitted(
    ContentAutofillDriver* source,
    FormData form,
    bool known_success,
    mojom::SubmissionSource submission_source,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     bool known_success,
                     mojom::SubmissionSource submission_source)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, form, known_success, submission_source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, known_success, submission_source);
}

void ContentAutofillRouter::TextFieldDidChange(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    base::TimeTicks timestamp,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box,
                     base::TimeTicks timestamp)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, bounding_box, timestamp);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, field, bounding_box, timestamp);
}

void ContentAutofillRouter::TextFieldDidScroll(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, bounding_box);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, field, bounding_box);
}

void ContentAutofillRouter::SelectControlDidChange(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, bounding_box);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, field, bounding_box);
}

void ContentAutofillRouter::AskForValuesToFill(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    FormElementWasClicked form_element_was_clicked,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box,
                     AutoselectFirstSuggestion autoselect_first_suggestion,
                     FormElementWasClicked form_element_was_clicked)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, bounding_box,
             autoselect_first_suggestion, form_element_was_clicked);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  SetLastQueriedSource(source);
  SetLastQueriedTarget(target);
  callback(target, *browser_form, field, bounding_box,
           autoselect_first_suggestion, form_element_was_clicked);
}

void ContentAutofillRouter::HidePopup(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // For Password Manager forms, |last_queried_target_| is not set. Since these
  // forms are not form-transcending, the we can unicast to the |source|.
  if (!last_queried_target_) {
    callback(source);
  } else {
    callback(last_queried_target_);
  }
}

void ContentAutofillRouter::FocusNoLongerOnForm(
    ContentAutofillDriver* source,
    bool had_interacted_form,
    void (*callback)(ContentAutofillDriver* target, bool had_interacted_form)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, had_interacted_form);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // Suppresses FocusNoLongerOnForm() if the focus has already moved to a
  // different frame.
  LocalFrameToken frame_token(
      source->render_frame_host()->GetFrameToken().value());
  if (focused_frame_ != frame_token)
    return;

  // Prevents FocusOnFormField() from calling FocusNoLongerOnForm().
  focus_no_longer_on_form_has_fired_ = true;

  TriggerReparseExcept(source);

  // TODO(crbug/1228706): Retrofit event with the FormGlobalId and unicast
  // event.
  ForEachFrame(form_forest_, [&](ContentAutofillDriver* some_driver) {
    callback(some_driver, had_interacted_form);
  });
}

void ContentAutofillRouter::FocusOnFormField(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const gfx::RectF& bounding_box,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const gfx::RectF& bounding_box)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, bounding_box);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  // Calls FocusNoLongerOnForm() if the focus has already moved from a
  // different frame and FocusNoLongerOnForm() hasn't been called yet.
  LocalFrameToken frame_token(
      source->render_frame_host()->GetFrameToken().value());
  if (focused_frame_ != frame_token && !focus_no_longer_on_form_has_fired_) {
    ForEachFrame(form_forest_, [&](ContentAutofillDriver* some_driver) {
      some_driver->FocusNoLongerOnFormCallback(true);
    });
  }

  // Suppresses late FocusNoLongerOnForm().
  focused_frame_ = frame_token;
  focus_no_longer_on_form_has_fired_ = false;

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, field, bounding_box);
}

void ContentAutofillRouter::DidFillAutofillFormData(
    ContentAutofillDriver* source,
    FormData form,
    base::TimeTicks timestamp,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     base::TimeTicks timestamp)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), timestamp);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  DCHECK(!last_queried_target_ ||
         last_queried_target_ == DriverOfFrame(browser_form->host_frame));
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form, timestamp);
}

void ContentAutofillRouter::DidPreviewAutofillFormData(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (last_queried_target_)
    callback(last_queried_target_);
}

void ContentAutofillRouter::DidEndTextFieldEditing(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  TriggerReparseExcept(source);

  // TODO(crbug/1228706): Retrofit event with the FormGlobalId and FieldGlobalId
  // and unicast event.
  ForEachFrame(form_forest_, callback);
}

void ContentAutofillRouter::SelectFieldOptionsDidChange(
    ContentAutofillDriver* source,
    FormData form,
    void (*callback)(ContentAutofillDriver* target, const FormData& form)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form));
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return );
  callback(target, *browser_form);
}

void ContentAutofillRouter::JavaScriptChangedAutofilledValue(
    ContentAutofillDriver* source,
    FormData form,
    const FormFieldData& field,
    const std::u16string& old_value,
    void (*callback)(ContentAutofillDriver* target,
                     const FormData& form,
                     const FormFieldData& field,
                     const std::u16string& old_value)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, std::move(form), field, old_value);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  FormGlobalId form_id = form.global_id();
  form_forest_.UpdateTreeOfRendererForm(std::move(form), source);

  TriggerReparseExcept(source);

  const FormData* browser_form = form_forest_.GetBrowserForm(form_id);
  AFCHECK(browser_form, return);
  auto* target = DriverOfFrame(browser_form->host_frame);
  AFCHECK(target, return);
  callback(target, *browser_form, field, old_value);
}

void ContentAutofillRouter::OnContextMenuShownInField(
    ContentAutofillDriver* source,
    const FormGlobalId& form_global_id,
    const FieldGlobalId& field_global_id,
    void (*callback)(ContentAutofillDriver* target,
                     const FormGlobalId& form_global_id,
                     const FieldGlobalId& field_global_id)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, form_global_id, field_global_id);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  TriggerReparseExcept(source);

  ForEachFrame(form_forest_, [&](ContentAutofillDriver* some_driver) {
    callback(some_driver, form_global_id, field_global_id);
  });
}

// Routing of events triggered by the browser.
//
// Below, `DriverOfFrame() == nullptr` does not necessarily indicate a bug and
// is therefore not NOTREACHED().
// The reason is that browser forms may be outdated and hence refer to frames
// that do not exist anymore.

std::vector<FieldGlobalId> ContentAutofillRouter::FillOrPreviewForm(
    ContentAutofillDriver* source,
    mojom::RendererFormDataAction action,
    const FormData& data,
    const url::Origin& triggered_origin,
    const base::flat_map<FieldGlobalId, ServerFieldType>& field_type_map,
    void (*callback)(ContentAutofillDriver* target,
                     mojom::RendererFormDataAction action,
                     const FormData& form)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, action, data);
    std::vector<FieldGlobalId> safe_fields;
    safe_fields.reserve(data.fields.size());
    for (const auto& field : data.fields)
      safe_fields.push_back(field.global_id());
    return safe_fields;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  internal::FormForest::RendererForms renderer_forms =
      form_forest_.GetRendererFormsOfBrowserForm(data, triggered_origin,
                                                 field_type_map);
  for (const FormData& renderer_form : renderer_forms.renderer_forms) {
    // Sending empty fill data to the renderer is semantically a no-op but
    // causes some further mojo calls.
    if (base::ranges::all_of(renderer_form.fields, &std::u16string::empty,
                             &FormFieldData::value)) {
      continue;
    }
    if (auto* target = DriverOfFrame(renderer_form.host_frame))
      callback(target, action, renderer_form);
  }
  return renderer_forms.safe_fields;
}

void ContentAutofillRouter::SendAutofillTypePredictionsToRenderer(
    ContentAutofillDriver* source,
    const std::vector<FormDataPredictions>& browser_fdps,
    void (*callback)(ContentAutofillDriver* target,
                     const std::vector<FormDataPredictions>& predictions)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, browser_fdps);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // Splits each FrameDataPredictions according to the respective FormData's
  // renderer forms, and groups these FormDataPredictions by the renderer form's
  // frame. We uso "fdp" as abbreviation of FormDataPredictions.
  std::map<LocalFrameToken, std::vector<FormDataPredictions>> renderer_fdps;
  for (const FormDataPredictions& browser_fdp : browser_fdps) {
    // Builds an index of the field predictions by the field's global ID.
    std::map<FieldGlobalId, FormFieldDataPredictions> field_predictions;
    DCHECK_EQ(browser_fdp.data.fields.size(), browser_fdp.fields.size());
    for (size_t i = 0; i < std::min(browser_fdp.data.fields.size(),
                                    browser_fdp.fields.size());
         ++i) {
      field_predictions.emplace(browser_fdp.data.fields[i].global_id(),
                                browser_fdp.fields[i]);
    }

    // Builds the FormDataPredictions of each renderer form and groups them by
    // the renderer form's frame in |renderer_fdps|.
    internal::FormForest::RendererForms renderer_forms =
        form_forest_.GetRendererFormsOfBrowserForm(
            browser_fdp.data, browser_fdp.data.main_frame_origin, {});
    for (FormData& renderer_form : renderer_forms.renderer_forms) {
      LocalFrameToken frame = renderer_form.host_frame;
      FormDataPredictions renderer_fdp;
      renderer_fdp.data = std::move(renderer_form);
      renderer_fdp.signature = browser_fdp.signature;
      for (const FormFieldData& field : renderer_fdp.data.fields) {
        renderer_fdp.fields.push_back(
            std::move(field_predictions[field.global_id()]));
      }
      renderer_fdps[frame].push_back(std::move(renderer_fdp));
    }
  }

  // Sends the predictions of the renderer forms to the individual frames.
  for (const auto& p : renderer_fdps) {
    LocalFrameToken frame = p.first;
    const std::vector<FormDataPredictions>& renderer_fdp = p.second;
    if (auto* target = DriverOfFrame(frame))
      callback(target, renderer_fdp);
  }
}

void ContentAutofillRouter::SendFieldsEligibleForManualFillingToRenderer(
    ContentAutofillDriver* source,
    const std::vector<FieldGlobalId>& fields,
    void (*callback)(ContentAutofillDriver* target,
                     const std::vector<FieldRendererId>& fields)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    std::vector<FieldRendererId> renderer_ids;
    renderer_ids.reserve(renderer_ids.size());
    for (FieldGlobalId field : fields)
      renderer_ids.push_back(field.renderer_id);
    callback(source, renderer_ids);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  // Splits FieldGlobalIds by their frames and reduce them to the
  // FieldRendererIds.
  std::map<LocalFrameToken, std::vector<FieldRendererId>> fields_by_frame;
  for (FieldGlobalId field : fields)
    fields_by_frame[field.frame_token].push_back(field.renderer_id);

  // Send the FieldRendererIds to the individual frames.
  for (const auto& p : fields_by_frame) {
    LocalFrameToken frame = p.first;
    const std::vector<FieldRendererId>& frame_fields = p.second;
    if (auto* target = DriverOfFrame(frame))
      callback(target, frame_fields);
  }
}

void ContentAutofillRouter::RendererShouldAcceptDataListSuggestion(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(ContentAutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, field.renderer_id, value);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, value);
  }
}

void ContentAutofillRouter::RendererShouldClearFilledSection(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  ForEachFrame(form_forest_, callback);
}

void ContentAutofillRouter::RendererShouldClearPreviewedForm(
    ContentAutofillDriver* source,
    void (*callback)(ContentAutofillDriver* target)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  ForEachFrame(form_forest_, callback);
}

void ContentAutofillRouter::RendererShouldFillFieldWithValue(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(ContentAutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, field.renderer_id, value);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (auto* target = DriverOfFrame(field.frame_token))
    callback(target, field.renderer_id, value);
}

void ContentAutofillRouter::RendererShouldPreviewFieldWithValue(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const std::u16string& value,
    void (*callback)(ContentAutofillDriver* target,
                     const FieldRendererId& field,
                     const std::u16string& value)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, field.renderer_id, value);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (auto* target = DriverOfFrame(field.frame_token))
    callback(target, field.renderer_id, value);
}

void ContentAutofillRouter::RendererShouldSetSuggestionAvailability(
    ContentAutofillDriver* source,
    const FieldGlobalId& field,
    const mojom::AutofillState state,
    void (*callback)(ContentAutofillDriver* target,
                     const FieldRendererId& field,
                     const mojom::AutofillState state)) {
  if (!base::FeatureList::IsEnabled(features::kAutofillAcrossIframes)) {
    callback(source, field.renderer_id, state);
    return;
  }

  some_rfh_for_debugging_ = source->render_frame_host()->GetGlobalId();

  if (auto* target = DriverOfFrame(field.frame_token)) {
    callback(target, field.renderer_id, state);
  }
}

}  // namespace autofill
