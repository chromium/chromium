// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_H_

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/field_filling_skip_reason.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {

struct Suggestion {
  struct PasswordSuggestionDetails {
    std::u16string username;
    std::u16string password;
    // The signon realm of the password. Unlike the `display_signon_realm`, it
    // is not necessarily user friendly/readable, but rather has the raw
    // `PasswordForm::signon_realm` value.
    std::string signon_realm;
    // Stores either the password signon realm or the Android app name for which
    // the password was saved.
    std::u16string display_signon_realm;
    // This flag is set to `false` for the manual fallback suggestions which
    // represent exact, strongly affiliated, PSL and weakly affiliated matches
    // for the domain the suggestions are shown for. All other suggestions have
    // this flag set to `true`.
    bool is_cross_domain = false;

    PasswordSuggestionDetails();
    PasswordSuggestionDetails(std::u16string_view username,
                              std::u16string_view password,
                              std::string_view signon_realm,
                              std::u16string_view display_signon_realm,
                              bool is_cross_domain);
    PasswordSuggestionDetails(const PasswordSuggestionDetails&);
    PasswordSuggestionDetails(PasswordSuggestionDetails&);
    PasswordSuggestionDetails& operator=(const PasswordSuggestionDetails&);
    PasswordSuggestionDetails& operator=(PasswordSuggestionDetails&&);
    virtual ~PasswordSuggestionDetails();

    friend bool operator==(const PasswordSuggestionDetails&,
                           const PasswordSuggestionDetails&) = default;
  };

  struct PlusAddressPayload final {
    PlusAddressPayload();
    explicit PlusAddressPayload(std::optional<std::u16string> address);
    PlusAddressPayload(const PlusAddressPayload&);
    PlusAddressPayload(PlusAddressPayload&&);
    PlusAddressPayload& operator=(const PlusAddressPayload&);
    PlusAddressPayload& operator=(PlusAddressPayload&&);
    ~PlusAddressPayload();

    friend bool operator==(const PlusAddressPayload&,
                           const PlusAddressPayload&) = default;

    // The proposed plus address string. If it is `nullopt`, then it is
    // currently loading and nothing is previewed.
    std::optional<std::u16string> address;
    // Whether the suggestion should display a refresh button.
    bool offer_refresh = true;
  };

  struct PredictionImprovementsPayload final {
    PredictionImprovementsPayload();
    PredictionImprovementsPayload(
        const base::flat_map<FieldGlobalId, std::u16string>& values_to_fill,
        const FieldTypeSet& field_types_to_fill,
        const DenseSet<FieldFillingSkipReason>& ignorable_skip_reasons);
    PredictionImprovementsPayload(const PredictionImprovementsPayload&);
    PredictionImprovementsPayload(PredictionImprovementsPayload&&);
    PredictionImprovementsPayload& operator=(
        const PredictionImprovementsPayload&);
    PredictionImprovementsPayload& operator=(PredictionImprovementsPayload&&);
    ~PredictionImprovementsPayload();

    friend bool operator==(const PredictionImprovementsPayload&,
                           const PredictionImprovementsPayload&) = default;

    // Values to be filled into fields with corresponding ids.
    base::flat_map<FieldGlobalId, std::u16string> values_to_fill;
    // Field types to be filled. Fields not matching a type in the set will be
    // skipped during filling.
    FieldTypeSet field_types_to_fill;
    // Autofill skip reasons that need to be ignored for filling improved
    // predictions.
    DenseSet<FieldFillingSkipReason> ignorable_skip_reasons;
  };

  struct PaymentsPayload final {
    PaymentsPayload();
    explicit PaymentsPayload(bool should_display_terms_available);
    PaymentsPayload(const PaymentsPayload&);
    PaymentsPayload(PaymentsPayload&&);
    PaymentsPayload& operator=(const PaymentsPayload&);
    PaymentsPayload& operator=(PaymentsPayload&&);
    ~PaymentsPayload();

    friend bool operator==(const PaymentsPayload&,
                           const PaymentsPayload&) = default;

    // If true, the user will be presented with a "Terms apply for card
    // benefits" message below the suggestions list on TTF for mobile.
    bool should_display_terms_available = false;
  };

  using IsLoading = base::StrongAlias<class IsLoadingTag, bool>;
  using Guid = base::StrongAlias<class GuidTag, std::string>;
  using InstrumentId = base::StrongAlias<class InstrumentIdTag, uint64_t>;
  using BackendId = absl::variant<Guid, InstrumentId>;
  using ValueToFill = base::StrongAlias<struct ValueToFill, std::u16string>;
  using Payload = absl::variant<BackendId,
                                GURL,
                                ValueToFill,
                                PasswordSuggestionDetails,
                                PlusAddressPayload,
                                PredictionImprovementsPayload,
                                PaymentsPayload>;

  // This struct is used to provide password suggestions with custom icons,
  // using the favicon of the website associated with the credentials. While
  // the favicon loads, the icon from `Suggestion::icon` will be used as
  // a placeholder.
  struct FaviconDetails {
    GURL domain_url;

    // Whether the favicon can be requested from a Google server. Only set to
    // `true` if the user's association with the domain is already known to
    // Google, e.g., because the user is syncing a credential for that domain.
    bool can_be_requested_from_google = false;

    friend bool operator==(const FaviconDetails&,
                           const FaviconDetails&) = default;
  };

  // This type is used to specify custom icons by providing a URL to the icon.
  // Used on Android only where `gfx::Image` (`custom_icon` alternative) is
  // not supported.
  using CustomIconUrl = base::StrongAlias<class CustomIconUrlTag, GURL>;

  // The text information shown on the UI layer for a Suggestion.
  struct Text {
    using IsPrimary = base::StrongAlias<class IsPrimaryTag, bool>;
    using ShouldTruncate = base::StrongAlias<class ShouldTruncateTag, bool>;

    Text();
    explicit Text(std::u16string value,
                  IsPrimary is_primary = IsPrimary(false),
                  ShouldTruncate should_truncate = ShouldTruncate(false));
    Text(const Text& other);
    Text(Text& other);
    Text& operator=(const Text& other);
    Text& operator=(Text&& other);
    ~Text();
    bool operator==(const Suggestion::Text& text) const = default;

    // The text value to be shown.
    std::u16string value;

    // Whether the text should be shown with a primary style.
    IsPrimary is_primary = IsPrimary(false);

    // Whether the text should be truncated if the bubble width is limited.
    ShouldTruncate should_truncate = ShouldTruncate(false);
  };

  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.ui.suggestion
  enum class Icon {
    kNoIcon,
    kAccount,
    // TODO(crbug.com/40266549): Rename to Undo.
    kClear,
    kCreate,
    kCode,
    kDelete,
    kDevice,
    kEdit,
    kEmail,
    kEmpty,
    kError,
    kGlobe,
    kGoogle,
    kGoogleMonochrome,
    kGooglePasswordManager,
    kGooglePay,
    kGooglePayDark,
    kHttpWarning,
    kHttpsInvalid,
    kKey,
    kLocation,
    kMagic,
    kOfferTag,
    kPenSpark,
    kPlusAddress,
    kScanCreditCard,
    kSettings,
    kSettingsAndroid,
    kUndo,
    // Payment method icons
    kCardGeneric,
    kCardAmericanExpress,
    kCardDiners,
    kCardDiscover,
    kCardElo,
    kCardJCB,
    kCardMasterCard,
    kCardMir,
    kCardTroy,
    kCardUnionPay,
    kCardVerve,
    kCardVisa,
    kIban,
    kAutofillPredictionImprovements,
    kAutofillPredictionImprovementsDark,
  };

  // This enum is used to control filtration of suggestions (see it's used in
  // the `PopupViewViews` search bar and `AutofillPopupControllerImpl` where
  // the logic is implemented) by explicitly marking special suggestions at
  // creation.
  enum class FiltrationPolicy {
    // Suggestions, that are normally filtered. The match is highlighted on
    // the UI and those that don't match are removed from the list.
    kFilterable,

    // Suggestions, that are excluded from filtration by always disappearing
    // from the list as soon as any filter is applied (in other words, these
    // suggestion don't match any filter except the empty one).
    kPresentOnlyWithoutFilter,

    // Suggestions, that are excluded from filtration by always staying in
    // in the list (basically, these suggestions ignore filter).
    kStatic,
  };

  // TODO(crbug.com/335194240): Consolidate expected param types for these
  // constructors. Some expect UTF16 strings and others UTF8, while internally
  // we only use UTF16. The ones expecting UTF8 are only used by tests and could
  // be easily refactored.
  Suggestion();
  explicit Suggestion(std::u16string main_text);
  explicit Suggestion(SuggestionType type);
  Suggestion(std::u16string main_text, SuggestionType type);
  // Constructor for unit tests. It will convert the strings from UTF-8 to
  // UTF-16.
  Suggestion(std::string_view main_text,
             std::string_view label,
             Icon icon,
             SuggestionType type);
  Suggestion(std::string_view main_text,
             std::vector<std::vector<Text>> labels,
             Icon icon,
             SuggestionType type);
  Suggestion(std::string_view main_text,
             std::string_view minor_text,
             std::string_view label,
             Icon icon,
             SuggestionType type);
  Suggestion(const Suggestion& other);
  Suggestion(Suggestion&& other);
  Suggestion& operator=(const Suggestion& other);
  Suggestion& operator=(Suggestion&& other);
  ~Suggestion();

  template <typename T>
  T GetPayload() const {
#if DCHECK_IS_ON()
    DCHECK(Invariant());
#endif
    return absl::holds_alternative<T>(payload) ? absl::get<T>(payload) : T{};
  }

  template <typename T>
  T GetBackendId() const {
    CHECK(absl::holds_alternative<BackendId>(payload));
    return absl::get<T>(absl::get<BackendId>(payload));
  }

#if DCHECK_IS_ON()
  bool Invariant() const {
    switch (type) {
      case SuggestionType::kCreateNewPlusAddressInline:
      case SuggestionType::kPlusAddressError:
        return absl::holds_alternative<PlusAddressPayload>(payload);
      case SuggestionType::kPasswordEntry:
        // Manual fallback password suggestions store the password to preview or
        // fill in the suggestion's payload. Regular per-domain contain empty
        // `BackendId`.
        // TODO(crbug.com/333992198): Use `PasswordSuggestionDetails` for all
        // suggestions with `SuggestionType::kPasswordEntry`.
        return absl::holds_alternative<BackendId>(payload) ||
               absl::holds_alternative<PasswordSuggestionDetails>(payload);
      case SuggestionType::kFillPassword:
      case SuggestionType::kViewPasswordDetails:
        return absl::holds_alternative<PasswordSuggestionDetails>(payload);
      case SuggestionType::kSeePromoCodeDetails:
        return absl::holds_alternative<GURL>(payload);
      case SuggestionType::kIbanEntry:
        return absl::holds_alternative<ValueToFill>(payload) ||
               absl::holds_alternative<BackendId>(payload);
      case SuggestionType::kFillPredictionImprovements:
        return absl::holds_alternative<ValueToFill>(payload) ||
               absl::holds_alternative<PredictionImprovementsPayload>(payload);
      case SuggestionType::kCreditCardEntry:
      case SuggestionType::kVirtualCreditCardEntry:
        // TODO(crbug.com/367434234): Use `PaymentsPayload` for all credit card
        // suggestions. Only Touch-To-Fill credit card suggestions currently
        // use this.
        return absl::holds_alternative<BackendId>(payload) ||
               absl::holds_alternative<PaymentsPayload>(payload);
      default:
        return absl::holds_alternative<BackendId>(payload);
    }
  }
#endif

  friend bool operator==(const Suggestion&, const Suggestion&) = default;

  // Payload generated by the backend layer. This payload contains the
  // information required for further actions after the suggestion is
  // selected/accepted. It can be either a GUID that identifies the exact
  // autofill profile that generated this suggestion, or a GURL that the
  // suggestion should navigate to upon being accepted, or a text that should be
  // shown other than main_text.
  Payload payload;

  // Determines popup identifier for the suggestion.
  SuggestionType type = SuggestionType::kAutocompleteEntry;

  // The texts that will be displayed on the first line in a suggestion. The
  // order of showing the two texts on the first line depends on whether it is
  // in RTL languages. The |main_text| includes the text value to be filled in
  // the form, while the |minor_text| includes other supplementary text value to
  // be shown also on the first line.
  Text main_text;
  Text minor_text;

  // The secondary texts displayed in a suggestion. The labels are presented as
  // a N*M matrix, and the position of the text in the matrix decides where the
  // text will be shown on the UI. (e.g. The text labels[1][2] will be shown on
  // the second line, third column in the grid view of label).
  std::vector<std::vector<Text>> labels;

  // Used only for passwords to show the credential signon realm if applicable.
  // Also used to display an extra line of information if two line
  // display is enabled.
  std::u16string additional_label;

  // This field outlines various methods for specifying the custom icon.
  // Depending on the use case and platform, it can be a `gfx::Image` instance
  // or imply more complex semantic of fetching the icon (see `CustomIconUrl`
  // and `FaviconDetails` docs for details).
  absl::variant<gfx::Image, CustomIconUrl, FaviconDetails> custom_icon;

  // The children of this suggestion. If present, the autofill popup will have
  // submenus.
  std::vector<Suggestion> children;
#if BUILDFLAG(IS_ANDROID)
  // On Android, the icon can be at the start of the suggestion before the label
  // or at the end of the label.
  bool is_icon_at_start = false;
  // TODO(crbug.com/346469807): Remove once strings are passed directly.
  std::u16string iph_description_text;
#endif  // BUILDFLAG(IS_ANDROID)

  // This is the icon which is shown on the side of a suggestion.
  // If |custom_icon| is empty, the fallback built-in icon.
  Icon icon = Icon::kNoIcon;

  // An icon that appears after the suggestion in the suggestion view. For
  // passwords, this icon string shows whether the suggestion originates from
  // local or account store. It is also used on the settings entry for the
  // credit card Autofill popup to indicate if all credit cards are server
  // cards. It also holds Google Password Manager icon on the settings entry for
  // the passwords Autofill popup.
  Icon trailing_icon = Icon::kNoIcon;

  // Whether suggestion was interacted with and is now in a loading state.
  IsLoading is_loading = IsLoading(false);

  // The In-Product-Help feature that should be shown for the suggestion.
  raw_ptr<const base::Feature> feature_for_iph = nullptr;

  // The feature for the new badge if one is supposed to be shown. Currently
  // available only on Desktop.
  raw_ptr<const base::Feature> feature_for_new_badge = nullptr;

  // If specified, this text will be played back as voice over for a11y.
  std::optional<std::u16string> voice_over;

  // If specified, this text will be played back if the user accepts this
  // suggestion.
  std::optional<std::u16string> acceptance_a11y_announcement;

  // When `type` is
  // `SuggestionType::k(Address|CreditCard)FieldByFieldFilling`, specifies the
  // `FieldType` used to build the suggestion's `main_text`.
  std::optional<FieldType> field_by_field_filling_type_used;

  // Whether the user is able to preview the suggestion by hovering on it or
  // accept it by clicking on it.
  bool is_acceptable = true;

  // How the suggestion should be handled by the filtration logic, see the enum
  // values doc for details.
  // Now used for filtering manually triggered password suggestions only and
  // has no effect on other suggestions.
  FiltrationPolicy filtration_policy = FiltrationPolicy::kFilterable;

  // If true, the user will see the suggestion in a "disabled and grayed-out"
  // form. This field should be true only when `is_acceptable` is false  which
  // will make the suggestion deactivated and unclickable.
  bool apply_deactivated_style = false;

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // If true, selecting a suggestion or, when it exists, expanding its
  // sub-popup, highlights the background of the suggestion row and its
  // contained cells.
  bool highlight_on_select = true;
#endif
};

std::string_view ConvertIconToPrintableString(Suggestion::Icon icon);
void PrintTo(const Suggestion& suggestion, std::ostream* os);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_SUGGESTION_H_
