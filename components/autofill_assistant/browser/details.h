// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_

#include <map>
#include <string>

#include "base/values.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {
class TriggerContext;

class Details {
 public:
  Details();
  ~Details();

  // Returns a dictionary describing the current execution context, which
  // is intended to be serialized as JSON string. The execution context is
  // useful when analyzing feedback forms and for debugging in general.
  base::Value GetDebugContext() const;

  // Update details from the given parameters. Returns true if changes were
  // made.
  // If one of the generic detail parameter is present then vertical specific
  // parameters are not used for Details creation.
  bool UpdateFromParameters(const TriggerContext& context);

  // Updates the details to show data directly from proto. Returns true if
  // |details| were successfully updated.
  static bool UpdateFromProto(const ShowDetailsProto& proto, Details* details);

  // Updates the details to show selected contact details. It shows only full
  // name and/or email, depending on |user_data_options|. Returns true if
  // |details| were successfully updated.
  static bool UpdateFromContactDetails(
      const ShowDetailsProto& proto,
      const UserData* user_data,
      const CollectUserDataOptions* user_data_options,
      Details* details);

  // Updates the details to show selected shipping details. It shows full name
  // and address. Returns true if |details| were successfully updated.
  static bool UpdateFromShippingAddress(const ShowDetailsProto& proto,
                                        const UserData* user_data,
                                        Details* details);

  // Updates the details to show credit card selected by the user. Returns true
  // if |details| were successfully updated.
  static bool UpdateFromSelectedCreditCard(const ShowDetailsProto& proto,
                                           const UserData* user_data,
                                           Details* details);

  const std::string title() const;
  int titleMaxLines() const;
  const std::string imageUrl() const;
  const base::Optional<std::string> imageAccessibilityHint() const;
  bool imageAllowClickthrough() const;
  const std::string imageDescription() const;
  const std::string imagePositiveText() const;
  const std::string imageNegativeText() const;
  const std::string imageClickthroughUrl() const;
  bool showImagePlaceholder() const;
  const std::string totalPriceLabel() const;
  const std::string totalPrice() const;
  const std::string descriptionLine1() const;
  const std::string descriptionLine2() const;
  const std::string descriptionLine3() const;
  const std::string priceAttribution() const;
  bool userApprovalRequired() const;
  bool highlightTitle() const;
  bool highlightLine1() const;
  bool highlightLine2() const;
  bool highlightLine3() const;
  bool animatePlaceholders() const;

  // Clears all change flags.
  void ClearChanges();

 private:
  void SetDetailsProto(const DetailsProto& proto);
  void SetDetailsChangesProto(const DetailsChangesProto& change_flags) {
    change_flags_ = change_flags;
  }

  // Tries updating the details using generic detail parameters. Returns true
  // if at least one generic detail parameter was found and used.
  bool MaybeUpdateFromDetailsParameters(const TriggerContext& context);

  // Updates fields by taking the current |proto_| values into account.
  void Update();

  DetailsProto proto_;
  DetailsChangesProto change_flags_;

  // Maximum of lines for the title.
  int title_max_lines_ = 1;

  // Content to be shown in description line 1 in the UI.
  std::string description_line_1_content_;

  // Content to be shown in description line 3 in the UI.
  std::string description_line_3_content_;

  // Content to be shown in the price attribution view in the UI.
  std::string price_attribution_content_;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_DETAILS_H_
