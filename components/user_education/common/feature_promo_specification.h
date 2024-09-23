// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_

#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/containers/flat_tree.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/user_education_metadata.h"
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
  // Represents additional conditions that can affect when a promo can show.
  class AdditionalConditions {
   public:
    AdditionalConditions();
    AdditionalConditions(AdditionalConditions&&) noexcept;
    AdditionalConditions& operator=(AdditionalConditions&&) noexcept;
    ~AdditionalConditions();

    // Provides constraints on when the promo can show based on some other
    // Feature Engagement event.
    enum class Constraint { kAtMost, kAtLeast, kExactly };

    // Represents an additional condition for the promo to show.
    struct AdditionalCondition {
      // The associated event name.
      std::string event_name;
      // How `count` should be interpreted.
      Constraint constraint = Constraint::kAtMost;
      // The required count for `event_name`, interpreted by `constraint`.
      uint32_t count = 0;
      // The window in which to evaluate `count` using `constraint`.
      std::optional<uint32_t> in_days;
    };

    // Sets the number of days in which "used" and other events should be
    // collected before deciding whether to show a promo.
    //
    // Default is zero unless there are additional conditions, in which case it
    // is a week.
    AdditionalConditions& set_initial_delay_days(uint32_t initial_delay_days) {
      this->initial_delay_days_ = initial_delay_days;
      return *this;
    }
    std::optional<uint32_t> initial_delay_days() const {
      return initial_delay_days_;
    }

    // Sets the number of times a promoted feature can be used before the
    // associated promo stops showing. Default is zero - i.e. if the feature is
    // used at all, the promo won't show.
    AdditionalConditions& set_used_limit(uint32_t used_limit) {
      this->used_limit_ = used_limit;
      return *this;
    }
    std::optional<uint32_t> used_limit() const { return used_limit_; }

    // Adds an additional constraint on when the promo can show. `event_name` is
    // arbitrary and can be shared between promos.
    //
    // Will only allow the promo to show if `event_name` has been seen
    // `constraint` `count` times in `in_days` days. If `in_days` isn't
    // specified, the period is effectively unlimited.
    void AddAdditionalCondition(const char* event_name,
                                Constraint constraint,
                                uint32_t count,
                                std::optional<uint32_t> in_days = std::nullopt);
    AdditionalConditions& AddAdditionalCondition(
        const AdditionalCondition& additional_condition);
    const std::vector<AdditionalCondition>& additional_conditions() const {
      return additional_conditions_;
    }

   private:
    std::optional<uint32_t> initial_delay_days_;
    std::optional<uint32_t> used_limit_;
    std::vector<AdditionalCondition> additional_conditions_;
  };

  // Provide different ways to specify parameters for title or body text.
  struct NoSubstitution {};
  using StringSubstitutions = std::vector<std::u16string>;
  using FormatParameters = std::variant<
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
    // Rotating promos have a list of different promos they cycle between.
    // Because they are shown over and over, possibly at startup, this type
    // requires being on an allowlist.
    kRotating = 6,
    kMaxValue = kRotating
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // Specifies the subtype of promo. Almost all promos will be `kNormal`; using
  // some of the other special types requires being on an allowlist.
  enum class PromoSubtype {
    // A normal promo. Follows the default rules for when it can show.
    kNormal = 0,
    // A promo designed to be shown per app or account, keyed to a unique
    // identifier. This type requires being on an allowlist.
    // (Previously known as "kPerApp".)
    kKeyedNotice = 1,
    // A promo that must be able to be shown until explicitly acknowledged and
    // dismissed by the user. This type requires being on an allowlist.
    kLegalNotice = 2,
    // A promo that must be able to be shown at most times, alerting the user
    // that something important has happened, and offering them an opportunity
    // to address it. This type requires being on an allowlist.
    kActionableAlert = 3,
    kMaxValue = kActionableAlert
  };

  // Represents a command or command accelerator. Can be valueless (falsy) if
  // neither a command ID nor an explicit accelerator is specified.
  class AcceleratorInfo {
   public:
    // You can assign either an int (command ID) or a ui::Accelerator to an
    // AcceleratorInfo object.
    using ValueType = std::variant<int, ui::Accelerator>;

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

  // A list of rotating promos. The order or index of promos should not change,
  // but a promo can be replaced with `std::nullopt` or another promo if it
  // becomes deprecated.
  class RotatingPromos {
   public:
    RotatingPromos();
    RotatingPromos(RotatingPromos&&) noexcept;
    RotatingPromos& operator=(RotatingPromos&&) noexcept;
    ~RotatingPromos();

    template <typename... Args>
    explicit RotatingPromos(Args&&... args) {
      (promos_.emplace_back(std::forward<Args>(args)), ...);
    }

    using ListType = std::vector<std::optional<FeaturePromoSpecification>>;
    using iterator = ListType::iterator;
    using const_iterator = ListType::const_iterator;
    using size_type = ListType::size_type;
    using value_type = ListType::value_type;
    using reference = ListType::reference;
    using const_reference = ListType::const_reference;

    iterator begin() { return promos_.begin(); }
    const_iterator begin() const { return promos_.begin(); }
    iterator end() { return promos_.end(); }
    const_iterator end() const { return promos_.end(); }
    reference operator[](size_type index) { return promos_[index]; }
    reference at(size_type index) { return promos_.at(index); }
    const_reference at(size_type index) const { return promos_.at(index); }
    size_type size() const { return promos_.size(); }

   private:
    ListType promos_;
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

  // Specifies a promo that shows a rotating set of promos.
  static FeaturePromoSpecification CreateForRotatingPromo(
      const base::Feature& feature,
      RotatingPromos rotating_promos);

  // Specifies a promo that shows a rotating set of promos.
  //
  // This is a convenience version of the method that allows each rotating promo
  // to be passed in as a list of items without boilerplate.
  template <typename Arg, typename... Args>
    requires std::same_as<std::remove_reference_t<Arg>,
                          std::optional<FeaturePromoSpecification>> ||
             std::same_as<std::remove_reference_t<Arg>,
                          FeaturePromoSpecification>
  static FeaturePromoSpecification CreateForRotatingPromo(
      const base::Feature& feature,
      Arg&& arg,
      Args&&... args) {
    return CreateForRotatingPromo(
        feature,
        RotatingPromos(std::forward<Arg>(arg), std::forward<Args>(args)...));
  }

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
  // For rotating promos, also sets the override on all sub-promos that are not
  // already explicitly set.
  //
  // You should document calls to this method with a reason and ideally a bug
  // describing why the default a11y behavior needs to be overridden and what
  // can be done to fix it.
  FeaturePromoSpecification& OverrideFocusOnShow(bool focus_on_show);

  // Set the promo subtype. Setting the subtype to most values other than
  // `kNormal` requires being on an allowlist.
  FeaturePromoSpecification& SetPromoSubtype(PromoSubtype promo_subtype);

  // For keyed and legal notice IPH, allows the promo to be re-shown under
  // specific circumstances. For keyed promos, the limit applies per key, not
  // the entire promo.
  //
  // There is a minimum allowed `reshow_delay` depending on promo type. The
  // current minimum delays are:
  //  - two weeks for "toast" promos
  //  - three months (90 days) for heavyweight promos
  //
  // The `max_show_count` is optional and can be used to limit the number of
  // times the promo can be shown, regardless of delay. If specified, this
  // count must be at least 2 (else it is meaningless).
  FeaturePromoSpecification& SetReshowPolicy(base::TimeDelta reshow_delay,
                                             std::optional<int> max_show_count);

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
  //
  // For rotating promos, call this method on the specific sub-promo.
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
  const std::optional<bool>& focus_on_show_override() const {
    return focus_on_show_override_;
  }
  int screen_reader_string_id() const { return screen_reader_string_id_; }
  const AcceleratorInfo& screen_reader_accelerator() const {
    return screen_reader_accelerator_;
  }
  const TutorialIdentifier& tutorial_id() const { return tutorial_id_; }
  const std::u16string custom_action_caption() const {
    return custom_action_caption_;
  }
  const std::optional<base::TimeDelta>& reshow_delay() const {
    return reshow_delay_;
  }
  const std::optional<int>& max_show_count() const { return max_show_count_; }

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

  // Sets the additional conditions for the promo to show.
  FeaturePromoSpecification& SetAdditionalConditions(
      AdditionalConditions additional_conditions);
  const AdditionalConditions& additional_conditions() const {
    return additional_conditions_;
  }

  // Sets the metadata for this promotion.
  FeaturePromoSpecification& SetMetadata(Metadata metadata);
  const Metadata& metadata() const { return metadata_; }

  // Argument-forwarding convenience version of SetMetadata() for constructing
  // a Metadata object in-place.
  template <typename... Args>
  FeaturePromoSpecification& SetMetadata(Args&&... args) {
    return SetMetadata(Metadata(std::forward<Args>(args)...));
  }

  // Only valid for rotating promo subtype.
  const RotatingPromos& rotating_promos() const { return rotating_promos_; }

  // Force the subtype to a particular value, bypassing permission checks.
  FeaturePromoSpecification& set_promo_subtype_for_testing(
      PromoSubtype promo_subtype) {
    promo_subtype_ = promo_subtype;
    return *this;
  }

  // Force the type of promo to rotating and set the given `rotating_promos`,
  // bypassing permission checks and safeguards.
  static FeaturePromoSpecification CreateRotatingPromoForTesting(
      const base::Feature& feature,
      RotatingPromos rotating_promos);

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

  // Whether and how many times the promo can reshow.
  std::optional<base::TimeDelta> reshow_delay_;
  std::optional<int> max_show_count_;

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
  std::optional<bool> focus_on_show_override_;

  // Optional screen reader announcement that replaces bubble text when the
  // bubble is first announced.
  int screen_reader_string_id_ = 0;

  // Accelerator that is used to fill in a parametric field in
  // screen_reader_string_id_.
  AcceleratorInfo screen_reader_accelerator_;

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

  // Additional conditions describing when the promo can show.
  AdditionalConditions additional_conditions_;

  // For rotating promos, maintain a list of sub-promos.
  RotatingPromos rotating_promos_;

  // Metadata for this promo.
  Metadata metadata_;
};

std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoSpecification::PromoType promo_type);
std::ostream& operator<<(std::ostream& oss,
                         FeaturePromoSpecification::PromoSubtype promo_subtype);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_SPECIFICATION_H_
