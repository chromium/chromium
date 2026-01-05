// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_H_

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <variant>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/strong_alias.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/common/unique_ids.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

struct Suggestion {
  struct PasswordSuggestionDetails {
    std::u16string username;
    std::u16string password;
    // Password used in the recovery flow initiated after failed password
    // change.
    std::optional<std::u16string> backup_password;
    // The signon realm of the password. Unlike the `display_signon_realm`, it
    // is not necessarily user friendly/readable, but rather has the raw
    // `PasswordForm::signon_realm` value.
    std::optional<std::string> signon_realm;
    // Stores either the password signon realm or the Android app name for which
    // the password was saved.
    std::optional<std::u16string> display_signon_realm;
    // This flag is set to `false` for the manual fallback suggestions which
    // represent exact, strongly affiliated, PSL and weakly affiliated matches
    // for the domain the suggestions are shown for. All other manual fallback
    // suggestions have this flag set to `true`.
    // Note that non-manual-fallback suggestions are never cross domain.
    bool is_cross_domain = false;

    PasswordSuggestionDetails();
    PasswordSuggestionDetails(std::u16string_view username,
                              std::u16string_view password,
                              std::string_view signon_realm,
                              std::u16string_view display_signon_realm,
                              bool is_cross_domain);
    // Used to construct the payload of a backup password suggestion.
    PasswordSuggestionDetails(std::u16string_view username,
                              std::u16string_view password,
                              std::u16string_view backup_password);
    PasswordSuggestionDetails(const PasswordSuggestionDetails&);
    PasswordSuggestionDetails(PasswordSuggestionDetails&&);
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

  struct AutofillAiPayload final {
    AutofillAiPayload();
    explicit AutofillAiPayload(EntityInstance::EntityId guid);
    AutofillAiPayload(const AutofillAiPayload&);
    AutofillAiPayload(AutofillAiPayload&&);
    AutofillAiPayload& operator=(const AutofillAiPayload&);
    AutofillAiPayload& operator=(AutofillAiPayload&&);
    ~AutofillAiPayload();

    friend bool operator==(const AutofillAiPayload&,
                           const AutofillAiPayload&) = default;

    EntityInstance::EntityId guid;
  };

  using Guid = base::StrongAlias<class GuidTag, std::string>;

  struct PaymentsPayload final {
    PaymentsPayload();
    PaymentsPayload(std::u16string main_text_content_description,
                    bool should_display_terms_available,
                    Guid guid,
                    bool is_local_payments_method);
    PaymentsPayload(const PaymentsPayload&);
    PaymentsPayload(PaymentsPayload&&);
    PaymentsPayload& operator=(const PaymentsPayload&);
    PaymentsPayload& operator=(PaymentsPayload&&);
    ~PaymentsPayload();

#if BUILDFLAG(IS_ANDROID)
    base::android::ScopedJavaLocalRef<jobject> CreateJavaObject() const;
#endif  // BUILDFLAG(IS_ANDROID)

    friend bool operator==(const PaymentsPayload&,
                           const PaymentsPayload&) = default;

    // Value to be announced for the suggestion's `main_text`.
    std::u16string main_text_content_description;

    // If true, the user will be presented with a "Terms apply for card
    // benefits" message below the suggestions list on TTF for mobile.
    bool should_display_terms_available = false;

    // Payments method identifier associated with suggestion.
    Guid guid;

    // If true, the payments method associated with the suggestion is local.
    bool is_local_payments_method = false;

    // The amount of the payment as extracted from the page. For example, used
    // for BNPL suggestions to confirm the amount is in the supported range for
    // a BNPL provider.
    std::optional<uint64_t> extracted_amount_in_micros;
  };

  struct AutofillProfilePayload final {
    AutofillProfilePayload();
    explicit AutofillProfilePayload(Guid guid);
    AutofillProfilePayload(Guid guid, std::u16string email_override);
    AutofillProfilePayload(const AutofillProfilePayload&);
    AutofillProfilePayload(AutofillProfilePayload&&);
    AutofillProfilePayload& operator=(const AutofillProfilePayload&);
    AutofillProfilePayload& operator=(AutofillProfilePayload&&);
    ~AutofillProfilePayload();

#if BUILDFLAG(IS_ANDROID)
    base::android::ScopedJavaLocalRef<jobject> CreateJavaObject() const;
#endif  // BUILDFLAG(IS_ANDROID)

    friend bool operator==(const AutofillProfilePayload&,
                           const AutofillProfilePayload&) = default;

    // Address profile identifier.
    Guid guid;
    // If non-empty, the email override is applied on the AutofillProfile
    // identified by `guid` every time it's loaded.
    std::u16string email_override;
  };

  struct IdentityCredentialPayload final {
    IdentityCredentialPayload();
    IdentityCredentialPayload(
        GURL configURL,
        std::string account_id,
        const std::map<FieldType, std::u16string>& fields);
    IdentityCredentialPayload(const IdentityCredentialPayload&);
    IdentityCredentialPayload(IdentityCredentialPayload&&);
    IdentityCredentialPayload& operator=(const IdentityCredentialPayload&);
    IdentityCredentialPayload& operator=(IdentityCredentialPayload&&);
    ~IdentityCredentialPayload();

    friend bool operator==(const IdentityCredentialPayload&,
                           const IdentityCredentialPayload&) = default;

    // The IdP's configURL as defined here:
    // https://w3c-fedid.github.io/FedCM/#dom-identityproviderconfig-configurl
    GURL config_url;
    // The account ID as defined here:
    // https://w3c-fedid.github.io/FedCM/#dom-identityprovideraccount-id
    std::string account_id;
    // The field values of the account profile available to autofill.
    std::map<FieldType, std::u16string> fields;
  };

  using IsLoading = base::StrongAlias<class IsLoadingTag, bool>;
  using InstrumentId = base::StrongAlias<class InstrumentIdTag, uint64_t>;
  using Payload = std::variant<Guid,
                               InstrumentId,
                               AutofillProfilePayload,
                               GURL,
                               PasswordSuggestionDetails,
                               PlusAddressPayload,
                               AutofillAiPayload,
                               PaymentsPayload,
                               IdentityCredentialPayload,
                               AutocompleteEntry>;

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

  // This struct is used to provide data for monochrome icons that are rendered
  // when there is no loyalty card program logo available.
  struct LetterMonochromeIcon {
    explicit LetterMonochromeIcon(std::u16string monogram_text)
        : monogram_text(monogram_text) {}

    friend bool operator==(const LetterMonochromeIcon&,
                           const LetterMonochromeIcon&) = default;

    // `monogram_text` is a std::u16string in order to support 2 letter
    // monograms.
    std::u16string monogram_text;
  };

  // This struct is used to provide the In-Product-Help bubble. It contains both
  // the feature and possibly params. Feature is for showing the suggestion and
  // params is needed for runtime texts.
  struct IPHMetadata {
    IPHMetadata();
    explicit IPHMetadata(const base::Feature* feature,
                         std::vector<std::u16string> iph_params = {});
    IPHMetadata(const IPHMetadata& iph_metadata);
    IPHMetadata(IPHMetadata&& iph_metadata);
    IPHMetadata& operator=(const IPHMetadata& iph_metadata);
    IPHMetadata& operator=(IPHMetadata&& iph_metadata);
    ~IPHMetadata();

    bool operator==(const IPHMetadata& b) const = default;
    auto operator<=>(const IPHMetadata& b) const = default;

    // The In-Product-Help feature that should be shown for the suggestion.
    // Present when a suggestion that will display an IPH bubble is created.
    raw_ptr<const base::Feature> feature = nullptr;

    // IPH params needed for runtime text replacements in the suggestion's IPH
    // bubble. Present when runtime texts need to be modified. It is empty when
    // the IPH strings has no placeholders.
    std::vector<std::u16string> iph_params;
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
    Text(Text&& other);
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
    kCode,
    kDelete,
    kDevice,
    kEdit,
    kEmail,
    kError,
    kFlight,
    kGlobe,
    kGoogle,
    kGoogleMonochrome,
    kGooglePasswordManager,
    kGooglePay,
    kGoogleWallet,
    kGoogleWalletMonochrome,
    kHome,
    kIdCard,
    kKey,
    kLocation,
    kLoyalty,
    kMagic,
    kOfferTag,
    kPenSpark,
    kPersonCheck,
    kPlusAddress,
    kQuestionMark,
    kRecoveryPassword,
    kScanCreditCard,
    kSettings,
    kUndo,
    kVehicle,
    kWork,
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
    kBnpl,
    kSaveAndFill,
    kAndroidMessages,
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

  // Describes whether a suggestion can be accepted and how it should be styled
  // when it cannot be.
  enum class Acceptability {
    // The suggestion can be accepted.
    kAcceptable,
    // The suggestion cannot be accepted (i.e. trying to accept it is ignored by
    // the UI controller).
    kUnacceptable,
    // The suggestion cannot be accepted and is displayed in a
    // disabled/grayed-out form.
    kUnacceptableWithDeactivatedStyle,
  };

  // TODO(crbug.com/335194240): Consolidate expected param types for these
  // constructors. Some expect UTF16 strings and others UTF8, while internally
  // we only use UTF16. The ones expecting UTF8 are only used by tests and could
  // be easily refactored.
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
             base::span<const std::string> minor_text_labels,
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
    return std::holds_alternative<T>(payload) ? std::get<T>(payload) : T{};
  }

#if DCHECK_IS_ON()
  bool Invariant() const {
    switch (type) {
      case SuggestionType::kIdentityCredential:
        return std::holds_alternative<IdentityCredentialPayload>(payload);
      case SuggestionType::kPasswordEntry:
        // Manual fallback password suggestions store the password to preview or
        // fill in the suggestion's payload.
        // TODO(crbug.com/333992198): Use `PasswordSuggestionDetails` only for
        // all suggestions with `SuggestionType::kPasswordEntry`.
        return std::holds_alternative<Guid>(payload) ||
               std::holds_alternative<PasswordSuggestionDetails>(payload);
      case SuggestionType::kFillPassword:
      case SuggestionType::kViewPasswordDetails:
      case SuggestionType::kBackupPasswordEntry:
      case SuggestionType::kTroubleSigningInEntry:
        return std::holds_alternative<PasswordSuggestionDetails>(payload);
      case SuggestionType::kSeePromoCodeDetails:
        return std::holds_alternative<GURL>(payload);
      case SuggestionType::kIbanEntry:
        return std::holds_alternative<Guid>(payload) ||
               std::holds_alternative<InstrumentId>(payload);
      case SuggestionType::kFillAutofillAi:
        return std::holds_alternative<AutofillAiPayload>(payload);
      case SuggestionType::kCreditCardEntry:
      case SuggestionType::kVirtualCreditCardEntry:
        // TODO(crbug.com/367434234): Use `PaymentsPayload` for all credit card
        // suggestions. Only Touch-To-Fill credit card suggestions currently
        // use this.
        return std::holds_alternative<Guid>(payload) ||
               std::holds_alternative<PaymentsPayload>(payload);
      case SuggestionType::kBnplEntry:
        return std::holds_alternative<PaymentsPayload>(payload);
      case SuggestionType::kDevtoolsTestAddressEntry:
      default:
        return std::holds_alternative<Guid>(payload) ||
               std::holds_alternative<AutofillProfilePayload>(payload);
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
  SuggestionType type;

  // The texts that will be displayed on the first line in a suggestion. The
  // order of showing the two texts on the first line depends on whether it is
  // in RTL languages. The `main_text` includes the text value to be filled in
  // the form, while `minor_texts` includes other supplementary text values
  // to be shown also on the first line.
  Text main_text;
  std::vector<Text> minor_texts;

  // The secondary texts displayed in a suggestion. The labels are presented as
  // a N*M matrix, and the position of the text in the matrix decides where the
  // text will be shown on the UI. (e.g. The text labels[1][2] will be shown on
  // the second line, third column in the grid view of label).
  std::vector<std::vector<Text>> labels;

  // Used only for passwords to:
  // 1. show the credential signon realm if applicable
  // 2. show the obfuscated backup password in a password recovery flow.
  // 3. show a "recovery" string next to the backup credential when displaying
  // backups alongside the primary credentials.
  // Also used to display an extra line of information if two line display is
  // enabled.
  std::u16string additional_label;

  // Whether the additional label is aligned to the right (or left in RTL).
  bool additional_label_alignment_right = false;

  // This field outlines various methods for specifying the custom icon.
  // Depending on the use case and platform, it can be a `gfx::Image` instance
  // or imply more complex semantic of fetching the icon (see `CustomIconUrl`,
  // `LetterMonochromeIcon` and `FaviconDetails` docs for details).
  std::variant<gfx::Image, CustomIconUrl, FaviconDetails, LetterMonochromeIcon>
      custom_icon;

  // The children of this suggestion. If present, the autofill popup will have
  // submenus.
  std::vector<Suggestion> children;
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/346469807): Remove once strings are passed directly.
  std::u16string iph_description_text;
#endif  // BUILDFLAG(IS_ANDROID)

  // This is the icon which is shown on the side of a suggestion.
  // If |custom_icon| is empty, the fallback built-in icon.
  Icon icon = Icon::kNoIcon;

#if BUILDFLAG(IS_IOS)
  // Indicates whether the suggestion has a custom card art image.
  bool has_custom_card_art_image = false;
#endif  // BUILDFLAG(IS_IOS)

  // An icon that appears after the suggestion in the suggestion view. For
  // passwords, this icon string shows whether the suggestion originates from
  // local or account store. It is also used on the settings entry for the
  // credit card Autofill popup to indicate if all credit cards are server
  // cards. It also holds Google Password Manager icon on the settings entry for
  // the passwords Autofill popup.
  Icon trailing_icon = Icon::kNoIcon;

  // Whether suggestion was interacted with and is now in a loading state.
  IsLoading is_loading = IsLoading(false);

  // The metadata needed for showing the In-Product-Help bubble.
  // TODO(crbug.com/369472865): Make this an std::optional<>.
  IPHMetadata iph_metadata;

  // The feature for the new badge if one is supposed to be shown. Currently
  // available only on Desktop.
  raw_ptr<const base::Feature> feature_for_new_badge = nullptr;

  // If specified, this text will be played back as voice over for a11y.
  std::optional<std::u16string> voice_over;

  // If specified, this text will be played back if the user accepts this
  // suggestion.
  std::optional<std::u16string> acceptance_a11y_announcement;

  // When `type` is
  // `SuggestionType::k(Address|CreditCard)FieldByFieldFilling` or
  // `SuggestionType::kAddressEntryOnTyping`, specifies the `FieldType` used to
  // build the suggestion's `main_text`.
  std::optional<FieldType> field_by_field_filling_type_used;

  // How the suggestion should be handled by the filtration logic, see the enum
  // values doc for details.
  // Now used for filtering manually triggered password suggestions only and
  // has no effect on other suggestions.
  FiltrationPolicy filtration_policy = FiltrationPolicy::kFilterable;

  // The acceptability of the suggestion, see the enum values doc for details.
  Acceptability acceptability = Acceptability::kAcceptable;

  // Returns whether the user is able to preview the suggestion by hovering on
  // it or accept it by clicking on it.
  bool IsAcceptable() const;

  // Returns whether the user will see the suggestion in
  // a "disabled and grayed-out" form.
  bool HasDeactivatedStyle() const;
};

void PrintTo(const Suggestion& suggestion, std::ostream* os);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_SUGGESTIONS_SUGGESTION_H_
