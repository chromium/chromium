// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ANDROID_TOUCH_TO_FILL_KEYBOARD_SUPPRESSOR_H_
#define COMPONENTS_AUTOFILL_ANDROID_TOUCH_TO_FILL_KEYBOARD_SUPPRESSOR_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_manager.h"

namespace autofill {

class ContentAutofillClient;

// Suppresses the Android IME.
//
// If a TTF surface intends to be displayed, it must suppress the keyboard
// preemptively before Autofill has parsed the form. Otherwise, the keyboard
// could be displayed while Autofill is parsing the form, which would lead to
// flickering.
//
// TouchToFillKeyboardSuppressor takes two callbacks from a TTF controller:
// - `is_showing()` returns true iff the TTF is currently shown;
// - `intends_to_show()` returns true iff the TTF is intended to be shown
//   once parsing is completed.
//
// With TouchToFillKeyboardSuppressor, the controller can implement the
// following behavior:
//
//                  +--------------------------------------------------------+
//                  |      After parsing: controller wants to show TTF?      |
//                  +--------------------------+-----------------------------+
//                  |            No            |            Yes              |
// +----------+-----+--------------------------+-----------------------------+
// | Before   | No  |                   Don't suppress.                      |
// | parsing: |     |                   Don't show TTF.                      |
// | con-     +-----+--------------------------+-----------------------------+
// | troller  | Yes | Suppress before parsing. | Suppress before parsing.    |
// | intends  |     | Unsuppress after parsing | Unsuppress after timeout if |
// | to show  |     | or after timeout.        | parsing is slow. Otherwise, |
// | TTF?     |     | Don't show TTF.          | show TTF, then unsuppress.  |
// +----------+-----+--------------------------+-----------------------------+
//
// The timeout mechanism is intended for cases where parsing takes extremely
// long.
//
// The TTF controller  must satisfy the following conditions:
// - The `is_showing()` callback must be true after parsing only if
//   `is_showing() || intends_to_show()` has been true before parsing.
// - The TTF controller must show the TTF only if `is_suppressing()` is true.
// - The TTF controller must call `Unsuppress()` after the TTF was shown.
//
// The lifecycle of a TouchToFillKeyboardSuppressor must be aligned to the
// lifecycle of a tab (represented here by a ContentAutofillClient).
// It must be created before any ContentAutofillDrivers of the tab have been
// created: it won't suppress the keyboard in frames that were created already.
//
// TouchToFillKeyboardSuppressor observes all AutofillManagers of a given tab
// (represented by the ContentAutofillClient). The event structure is this:
// 1. AutofillManager::Observer::OnBeforeAskForValuesToFill().
// 2. Asynchronous parsing.
// 3. Controller's shows TTF only if
//    - `intends_to_show()` had been true in Step 1, and
//    - `is_suppressing()` is true now.
// 4. AutofillManager::Observer::OnAfterAskForValuesToFill().
class TouchToFillKeyboardSuppressor
    : public ContentAutofillDriverFactory::Observer,
      public AutofillManager::Observer {
 public:
  explicit TouchToFillKeyboardSuppressor(
      ContentAutofillClient* autofill_client,
      base::RepeatingCallback<bool(AutofillManager&)> is_showing,
      base::RepeatingCallback<
          bool(AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&)>
          intends_to_show,
      base::TimeDelta timeout);
  TouchToFillKeyboardSuppressor(const TouchToFillKeyboardSuppressor&) = delete;
  TouchToFillKeyboardSuppressor& operator=(
      const TouchToFillKeyboardSuppressor&) = delete;
  ~TouchToFillKeyboardSuppressor() override;

  // ContentAutofillDriverFactory::Observer:
  void OnContentAutofillDriverFactoryDestroyed(
      ContentAutofillDriverFactory& factory) override;
  void OnContentAutofillDriverCreated(ContentAutofillDriverFactory& factory,
                                      ContentAutofillDriver& driver) override;

  // AutofillManager::Observer:
  void OnAutofillManagerStateChanged(
      AutofillManager& manager,
      AutofillManager::LifecycleState old_state,
      AutofillManager::LifecycleState new_state) override;
  void OnBeforeAskForValuesToFill(AutofillManager& manager,
                                  FormGlobalId form_id,
                                  FieldGlobalId field_id,
                                  const FormData& form_data) override;
  void OnAfterAskForValuesToFill(AutofillManager& manager,
                                 FormGlobalId form_id,
                                 FieldGlobalId field_id) override;

  // Returns true iff some AutofillManager's keyboard is currently suppressed.
  bool is_suppressing() { return suppressed_manager_.get(); }

  // Unsuppresses the keyboard if it's currently being suppressed.
  // This does not raise the keyboard itself.
  void Unsuppress();

 private:
  void KeepSuppressing() { unsuppress_timer_.Stop(); }
  void Suppress(AutofillManager& manager);

  base::RepeatingCallback<bool(AutofillManager&)> is_showing_;
  base::RepeatingCallback<
      bool(AutofillManager&, FormGlobalId, FieldGlobalId, const FormData&)>
      intends_to_show_;

  base::ScopedObservation<ContentAutofillDriverFactory,
                          ContentAutofillDriverFactory::Observer>
      driver_factory_observation_{this};
  base::ScopedMultiSourceObservation<AutofillManager, AutofillManager::Observer>
      autofill_manager_observations_{this};

  // The single AutofillManager whose keyboard is currently suppressed by this
  // suppressor; `nullptr` if no AutofillManager's keyboard is suppressed.
  // A raw pointer suffices because because OnAutofillManagerStateChanged()
  // resets it if necessary.
  raw_ptr<AutofillManager> suppressed_manager_ = nullptr;

  // Unsuppresses the keyboard after `timeout_`.
  base::OneShotTimer unsuppress_timer_;
  base::TimeDelta timeout_;

  base::WeakPtrFactory<TouchToFillKeyboardSuppressor> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_ANDROID_TOUCH_TO_FILL_KEYBOARD_SUPPRESSOR_H_
