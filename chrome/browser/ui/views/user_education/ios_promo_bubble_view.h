// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IOS_PROMO_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IOS_PROMO_BUBBLE_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/promos/ios_promo_constants.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/help_bubble/custom_help_bubble.h"
#include "components/user_education/common/user_education_context.h"
#include "content/public/browser/page_navigator.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

class Profile;

namespace syncer {
class DeviceInfo;
}  // namespace syncer

// A view for the bubble promo that encourages feature usage on iOS. This is
// intended to be used with the user education service.
class IOSPromoBubbleView : public views::BubbleDialogDelegateView,
                           public user_education::CustomHelpBubbleUi {
  METADATA_HEADER(IOSPromoBubbleView, views::View)

 public:
  using OpenUrlCallback =
      base::RepeatingCallback<void(const content::OpenURLParams&)>;

  // Factory method to create an IOSPromoBubbleView.
  static std::unique_ptr<IOSPromoBubbleView> Create(
      desktop_to_mobile_promos::PromoType promo_type,
      const scoped_refptr<user_education::UserEducationContext>& context,
      user_education::FeaturePromoSpecification::BuildHelpBubbleParams params);

  IOSPromoBubbleView(Profile* profile,
                     desktop_to_mobile_promos::PromoType promo_type,
                     desktop_to_mobile_promos::BubbleType promo_bubble_type,
                     views::View* anchor_view,
                     views::BubbleBorder::Arrow arrow);
  ~IOSPromoBubbleView() override;

  // BubbleDialogDelegate:
  bool Cancel() override;
  bool Accept() override;

  // views::View:
  void AddedToWidget() override;
  void VisibilityChanged(View* starting_from, bool is_visible) override;

  // Set the handle to the open url callback for testing.
  void SetOpenUrlCallbackForTesting(OpenUrlCallback callback);

 private:
  // Sets the width based on the given metric.
  void SetWidth(views::DistanceMetric metric);

  // Called when the bubble is dismissed.
  void OnDismissal();

  // Updates the bubble content to show the reminder confirmation message.
  void ShowReminderConfirmation();

  // Returns the formatted description string for the reminder confirmation
  // view.
  std::u16string GetConfirmationDescriptionText(
      const std::u16string& device_name);

  const raw_ptr<Profile> profile_;
  const desktop_to_mobile_promos::PromoType promo_type_;
  raw_ptr<const syncer::DeviceInfo> ios_device_info_;
  // The type of bubble being displayed. Can be changed from kReminder to
  // kReminderConfirmation.
  desktop_to_mobile_promos::BubbleType promo_bubble_type_;
  IOSPromoConstants::IOSPromoTypeConfigs config_;

  // The callback to open the url for testing.
  OpenUrlCallback open_url_callback_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_USER_EDUCATION_IOS_PROMO_BUBBLE_VIEW_H_
