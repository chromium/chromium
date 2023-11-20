// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"

namespace gfx {
struct VectorIcon;
}

namespace user_education {

class FeaturePromoHandle;

// Specifies the parameters for a feature promo and its associated bubble.
class FeaturePromoSpecification {
 public:
  // Provide different ways to specify parameters for title or body text.
  struct NoSubstitution {};
  using StringSubstitutions = std::vector<std::u16string>;
  using FormatParameters = absl::variant<
      // No substitutions; use the string as-is (default).
      NoSubstitution,
      // Use the following substitutions for the various substitution fields.
      StringSubstitutions,
      // Use a single string substitution. Included for convenience.
      std::u16string,
      // Specify a number of items in a singular/plural string.
      int>;

  // Optional method that filters a set of potential `elements` to choose and
  // return the anchor element, or null if none of the inputs is appropriate.
  // This method can return an element different from the input list, or null
  // if no valid element is found (this will cause the IPH not to run).
  using AnchorElementFilter = base::RepeatingCallback<ui::TrackedElement*(
      const ui::ElementTracker::ElementList& elements)>;

  // The callback type when creating a custom action IPH. The parameters are
  // `context`, which provides the context of the window in which the promo was
  // shown, and `promo_handle`, which holds the promo open until it is
  // destroyed.
  //
  // Typically, if you are taking an additional sequence of actions in response
  // to the custom callback, you will want to move `promo_handle` into longer-
  // term storage until that sequence is complete, to prevent additional IPH or
  // similar promos from being able to trigger in the interim. If you do not
  // care, simply let `promo_handle` expire at the end of the callback.
  using CustomActionCallback =
      base::RepeatingCallback<void(ui::ElementContext context,
                                   FeaturePromoHandle promo_handle)>;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Describes the type of promo. Used to configure defaults for the promo's
  // bubble.
  enum class PromoType {
    // Uninitialized/invalid specification.
    kUnspecified = 0,
    // A toast-style promo.
    kToast = 1,
    // A snooze-style promo.
    kSnooze = 2,
    // A tutorial promo.
    kTutorial = 3,
    // A promo where one button is replaced by a custom action.
    kCustomAction = 4,
    // A simple promo that acts like a toast but without the required
    // accessibility data.
    kLegacy = 5,
    kMaxValue = kLegacy
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Specifies the subtype of promo. Almost all promos will be `kNormal`; using
  // some of the other special types requires being on an allowlist.
  enum class PromoSubtype {
    // A normal promo. Follows the default rules for when it can show.
    kNormal = 0,
    // A promo designed to be shown in multiple apps (or webapps). Can show once
    // per app.
    kPerApp = 1,
    // A promo that must be able to be shown until explicitly acknowledged and
    // dismissed by the user. This type requires being on an allowlist.
    kLegalNotice = 2,
    kMaxValue = kLegalNotice
  };

  // Represents a command or command accelerator. Can be valueless (falsy) if
  // neither a command ID nor an explicit accelerator is specified.
  class AcceleratorInfo {
   public:
    // You can assign either an int (command ID) or a ui::Accelerator to an
    // AcceleratorInfo object.
    using ValueType = absl::variant<int, ui::Accelerator>;

    AcceleratorInfo();
    AcceleratorInfo(const AcceleratorInfo& other);
    explicit AcceleratorInfo(ValueType value);
    AcceleratorInfo& operator=(ValueType value);
    AcceleratorInfo& operator=(const AcceleratorInfo& other);
    ~AcceleratorInfo();

    explicit operator bool() const;
    bool operator!() const { return !static_cast<bool>(*this); }

    ui::Accelerator GetAccelerator(
        const ui::AcceleratorProvider* provider) const;

   private:
    ValueType value_;
  };

  struct DemoPageInfo {
    std::string display_title;
    std::string display_description;
    base::RepeatingClosure setup_for_feature_promo_callback;

    explicit DemoPageInfo(
        std::string display_title_ = std::string(),
        std::string display_description_ = std::string(),
        base::RepeatingClosure setup_for_feature_promo_callback_ =
            base::DoNothing());
    DemoPageInfo(const DemoPageInfo& other);
    DemoPageInfo& operator=(const DemoPageInfo& other);
    ~DemoPageInfo();
  };

  FeaturePromoSpecification();
  FeaturePromoSpecification(FeaturePromoSpecification&& other) noexcept;
  FeaturePromoSpecification& operator=(
      FeaturePromoSpecification&& other) noexcept;
  ~FeaturePromoSpecification();

  // Format a localized string with ID `string_id` based on the given
  // `format_params`.
  static std::u16string FormatString(int string_id,
                                     const FormatParameters& format_params);

  // Specifies a standard toast promo.
  //
  // Because toasts are transient and time out after a short period, it can be
  // difficult for screen reader users to navigate to the UI they point to.
  // Because of this, toasts require a screen reader prompt that is different
  // from the bubble text. This prompt should fully describe the UI the toast is
  // pointing to, and may include a single parameter, which is the accelerator
  // that is used to open/access the UI.
  //
  // For example, for a promo for the bookmark star, you might have:
  // Bubble text: "Click here to bookmark the current tab."
  // Accessible text: "Press |<ph name="ACCEL">$1<ex>Ctrl+D</ex></ph>| "
  //                  "to bookmark the current tab"
  // Accelerator: AcceleratorInfo(IDC_BOOKMARK_THIS_TAB)
  //
  // In this case, the system-specific accelerator for IDC_BOOKMARK_THIS_TAB is
  // retrieved and its text representation is injected into the accessible text
  // for screen reader users. An empty `AcceleratorInfo()` can be used for cases
  // where the accessible text does not require an accelerator.
  static FeaturePromoSpecification CreateForToastPromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      int accessible_text_string_id,
      AcceleratorInfo accessible_accelerator);

  // Specifies a promo with snooze buttons.
  static FeaturePromoSpecification CreateForSnoozePromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id);

  // Specifies a promo with snooze buttons, but with accessible text string id.
  // See comments from `FeaturePromoSpecification::CreateForToastPromo()`.
  static FeaturePromoSpecification CreateForSnoozePromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      int accessible_text_string_id,
      AcceleratorInfo accessible_accelerator);

  // Specifies a promo that launches a tutorial.
  static FeaturePromoSpecification CreateForTutorialPromo(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      TutorialIdentifier tutorial_id);

  // Specifies a promo that triggers a custom action.
  static FeaturePromoSpecification CreateForCustomAction(
      const base::Feature& feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id,
      int custom_action_string_id,
      CustomActionCallback custom_action_callback);

  // Specifies a text-only promo without additional accessibility information.
  // Deprecated. Only included for backwards compatibility with existing
  // promos. This is the only case in which |feature| can be null, and if it is
  // the result can only be used for a critical promo.
  static FeaturePromoSpecification CreateForLegacyPromo(
      const base::Feature* feature,
      ui::ElementIdentifier anchor_element_id,
      int body_text_string_id);

  // Set the optional bubble title. This text appears above the body text in a
  // slightly larger font.
  FeaturePromoSpecification& SetBubbleTitleText(int title_text_string_id);

  // Set the optional bubble icon. This is displayed next to the title or body
  // text.
  FeaturePromoSpecification& SetBubbleIcon(const gfx::VectorIcon* bubble_icon);

  // Set the bubble arrow. Default is top-left.
  FeaturePromoSpecification& SetBubbleArrow(HelpBubbleArrow bubble_arrow);

  // Overrides the default focus-on-show behavior for the bubble. By default
  // bubbles with action buttons are focused to aid with accessibility. In
  // unusual circumstances this allows the value to be overridden. However, it
  // is almost always better to e.g. improve the promo trigger logic so it
  // doesn't interrupt user workflow than it is to disable bubble auto-focus.
  //
  // You should document calls to this method with a reason and ideally a bug
  // describing why the default a11y behavior needs to be overridden and what
  // can be done to fix it.
  FeaturePromoSpecification& OverrideFocusOnShow(bool focus_on_show);

  // Set the promo subtype. Setting the subtype to LegalNotice requires being on
  // an allowlist.
  FeaturePromoSpecification& SetPromoSubtype(PromoSubtype promo_subtype);

  // Set the anchor element filter.
  FeaturePromoSpecification& SetAnchorElementFilter(
      AnchorElementFilter anchor_element_filter);

  // Set whether we should look for the anchor element in any context.
  // Default is false. Since usually we only want to create the bubble in the
  // currently active window, this is only really useful for cases where there
  // is a floating window, WebContents, or tab-modal dialog that can become
  // detached from the current active window and therefore requires its own
  // unique context.
  FeaturePromoSpecification& SetInAnyContext(bool in_any_context);

  // Get the anchor element based on `anchor_element_id`,
  // `anchor_element_filter`, and `context`.
  ui::TrackedElement* GetAnchorElement(ui::ElementContext context) const;

  const base::Feature* feature() const { return feature_; }
  PromoType promo_type() const { return promo_type_; }
  PromoSubtype promo_subtype() const { return promo_subtype_; }
  ui::ElementIdentifier anchor_element_id() const { return anchor_element_id_; }
  const AnchorElementFilter& anchor_element_filter() const {
    return anchor_element_filter_;
  }
  bool in_any_context() const { return in_any_context_; }
  int bubble_body_string_id() const { return bubble_body_string_id_; }
  int bubble_title_string_id() const { return bubble_title_string_id_; }
  const gfx::VectorIcon* bubble_icon() const { return bubble_icon_; }
  HelpBubbleArrow bubble_arrow() const { return bubble_arrow_; }
  const absl::optional<bool>& focus_on_show_override() const {
    return focus_on_show_override_;
  }
  int screen_reader_string_id() const { return screen_reader_string_id_; }
  const AcceleratorInfo& screen_reader_accelerator() const {
    return screen_reader_accelerator_;
  }
  const DemoPageInfo& demo_page_info() const { return demo_page_info_; }
  FeaturePromoSpecification& SetDemoPageInfo(DemoPageInfo demo_page_info);
  const TutorialIdentifier& tutorial_id() const { return tutorial_id_; }
  const std::u16string custom_action_caption() const {
    return custom_action_caption_;
  }

  // Sets whether the custom action button is the default button on the help
  // bubble (default is false). It is an error to call this method for a promo
  // not created with CreateForCustomAction().
  FeaturePromoSpecification& SetCustomActionIsDefault(
      bool custom_action_is_default);
  bool custom_action_is_default() const { return custom_action_is_default_; }

  // Used to claim the callback when creating the bubble.
  CustomActionCallback custom_action_callback() const {
    return custom_action_callback_;
  }
  FeaturePromoSpecification& SetCustomActionDismissText(
      int custom_action_dismiss_string_id);
  int custom_action_dismiss_string_id() const {
    return custom_action_dismiss_string_id_;
  }

  // Set menu item element identifiers that should be highlighted while
  // this FeaturePromo is active.
  FeaturePromoSpecification& SetHighlightedMenuItem(
      const ui::ElementIdentifier highlighted_menu_identifier);
  const ui::ElementIdentifier highlighted_menu_identifier() const {
    return highlighted_menu_identifier_;
  }

  // Force the subtype to a particular value, bypassing permission checks.
  void set_promo_subtype_for_testing(PromoSubtype promo_subtype) {
    promo_subtype_ = promo_subtype;
  }

 private:
  static constexpr HelpBubbleArrow kDefaultBubbleArrow =
      HelpBubbleArrow::kTopRight;

  FeaturePromoSpecification(const base::Feature* feature,
                            PromoType promo_type,
                            ui::ElementIdentifier anchor_element_id,
                            int bubble_body_string_id);

  raw_ptr<const base::Feature> feature_ = nullptr;

  // The type of promo. A promo with type kUnspecified is not valid.
  PromoType promo_type_ = PromoType::kUnspecified;

  // The subtype of the promo.
  PromoSubtype promo_subtype_ = PromoSubtype::kNormal;

  // The element identifier of the element to attach the promo to.
  ui::ElementIdentifier anchor_element_id_;

  // Whether we are allowed to search for the anchor element in any context.
  bool in_any_context_ = false;

  // The filter to use if there is more than one matching element, or
  // additional processing is needed (default is to always use the first
  // matching element).
  AnchorElementFilter anchor_element_filter_;

  // Text to be displayed in the promo bubble body. Should not be zero for
  // valid bubbles. We keep the string ID around because we can specify format
  // parameters when we actually create the bubble.
  int bubble_body_string_id_ = 0;

  // Optional text that is displayed at the top of the bubble, in a slightly
  // more prominent font.
  int bubble_title_string_id_ = 0;

  // Optional icon that is displayed next to bubble text.
  raw_ptr<const gfx::VectorIcon> bubble_icon_ = nullptr;

  // Optional arrow pointing to the promo'd element. Defaults to top left.
  HelpBubbleArrow bubble_arrow_ = kDefaultBubbleArrow;

  // Overrides the default focus-on-show behavior for a bubble, which is to
  // focus bubbles with action buttons, but not bubbles that only have a close
  // button.
  absl::optional<bool> focus_on_show_override_;

  // Optional screen reader announcement that replaces bubble text when the
  // bubble is first announced.
  int screen_reader_string_id_ = 0;

  // Accelerator that is used to fill in a parametric field in
  // screen_reader_string_id_.
  AcceleratorInfo screen_reader_accelerator_;

  // Information to be displayed on the demo page
  DemoPageInfo demo_page_info_;

  // Tutorial identifier if the user decides to view a tutorial.
  TutorialIdentifier tutorial_id_;

  // Custom action button text.
  std::u16string custom_action_caption_;

  // Custom action button action.
  CustomActionCallback custom_action_callback_;

  // Whether the custom action is the default button.
  bool custom_action_is_default_ = false;

  // Dismiss string ID for the custom action promo.
  int custom_action_dismiss_string_id_;

  // Identifier of the menu item that should be highlighted while
  // FeaturePromo is active.
  ui::ElementIdentifier highlighted_menu_identifier_;
};

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoSpecification::PromoType promo_type);
std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoSpecification::PromoSubtype promo_subtype);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_
