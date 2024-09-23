// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/android/touch_to_fill_keyboard_suppressor.h"

#include "base/check_op.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "content/public/browser/render_widget_host.h"

namespace autofill {

TouchToFillKeyboardSuppressor::TouchToFillKeyboardSuppressor(
    ContentAutofillClient* autofill_client,
    base::RepeatingCallback<bool(AutofillManager&)> is_showing,
    base::RepeatingCallback<
        bool(AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&)>
        intends_to_show,
    base::TimeDelta timeout)
    : is_showing_(std::move(is_showing)),
      intends_to_show_(std::move(intends_to_show)),
      timeout_(timeout) {
  // The keyboard suppressor behaves properly only if no AutofillDrivers (and no
  // AutofillManagers) have been created yet.
  CHECK_EQ(autofill_client->GetAutofillDriverFactory().num_drivers(), 0u);
  driver_factory_observation_.Observe(
      &autofill_client->GetAutofillDriverFactory());
}

TouchToFillKeyboardSuppressor::~TouchToFillKeyboardSuppressor() {
  Unsuppress();
  CHECK(!is_suppressing());
}

void TouchToFillKeyboardSuppressor::OnContentAutofillDriverFactoryDestroyed(
    ContentAutofillDriverFactory& factory) {
  Unsuppress();
  driver_factory_observation_.Reset();
}

void TouchToFillKeyboardSuppressor::OnContentAutofillDriverCreated(
    ContentAutofillDriverFactory& factory,
    ContentAutofillDriver& driver) {
  // GetRenderWidgetHost() returns the RWH attached to `rfh` or the nearest
  // ancestor frame. Since the ancestor frames have been processed by this
  // function already, the callback has been registered with `rwh` already iff
  // the parent frame's RWH is identical to `rwh`.
  content::RenderFrameHost* rfh = driver.render_frame_host();
  DCHECK_EQ(rfh->GetParent() != nullptr, driver.GetParent() != nullptr);
  content::RenderWidgetHost* rwh = rfh->GetRenderWidgetHost();
  if (!rfh->GetParent() || rfh->GetParent()->GetRenderWidgetHost() != rwh) {
    // We don't need to call RWH::RemoveSuppressShowingImeCallback(): it's
    // memory-safe due to the WeakPtr, and it's not a memory leak because
    // TouchToFillKeyboardSuppressor's lifecycle is aligned with the tab.
    rwh->AddSuppressShowingImeCallback(base::BindRepeating(
        [](base::WeakPtr<TouchToFillKeyboardSuppressor> self) {
          return self && self->is_suppressing();
        },
        weak_ptr_factory_.GetWeakPtr()));
  }
  autofill_manager_observations_.AddObservation(&driver.GetAutofillManager());
}

void TouchToFillKeyboardSuppressor::OnAutofillManagerStateChanged(
    AutofillManager& manager,
    AutofillManager::LifecycleState old_state,
    AutofillManager::LifecycleState new_state) {
  switch (new_state) {
    case AutofillManager::LifecycleState::kInactive:
    case AutofillManager::LifecycleState::kActive:
    case AutofillManager::LifecycleState::kPendingReset:
      break;
    case AutofillManager::LifecycleState::kPendingDeletion:
      if (suppressed_manager_.get() == &manager) {
        Unsuppress();
      }
      autofill_manager_observations_.RemoveObservation(&manager);
      break;
  }
}

void TouchToFillKeyboardSuppressor::OnBeforeAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id,
    const FormData& form_data) {
  if (is_showing_.Run(manager) ||
      intends_to_show_.Run(manager, form_id, field_id, form_data)) {
    Suppress(manager);
  } else {
    Unsuppress();
  }
}

void TouchToFillKeyboardSuppressor::OnAfterAskForValuesToFill(
    AutofillManager& manager,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  if (is_showing_.Run(manager)) {
    KeepSuppressing();
  } else {
    Unsuppress();
  }
}

void TouchToFillKeyboardSuppressor::Suppress(AutofillManager& manager) {
  if (suppressed_manager_.get() == &manager) {
    return;
  }
  Unsuppress();
  suppressed_manager_ = &manager;
  unsuppress_timer_.Start(FROM_HERE, timeout_, this,
                          &TouchToFillKeyboardSuppressor::Unsuppress);
}

void TouchToFillKeyboardSuppressor::Unsuppress() {
  if (!suppressed_manager_) {
    return;
  }
  suppressed_manager_ = nullptr;
  unsuppress_timer_.Stop();
}

}  // namespace autofill
