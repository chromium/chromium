// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include <unordered_set>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"

namespace autofill::autofill_metrics {

namespace {

std::string_view GetMetadataAvailabilitySuffix(
    const CardMetadataLoggingContext& context) {
  if (context.card_product_description_shown && context.card_art_image_shown) {
    return kProductNameAndArtImageBothShownSuffix;
  }
  if (context.card_product_description_shown) {
    return kProductNameShownOnlySuffix;
  }
  if (context.card_art_image_shown) {
    return kArtImageShownOnlySuffix;
  }
  return kProductNameAndArtImageNotShownSuffix;
}

}  // namespace

CardMetadataLoggingContext::CardMetadataLoggingContext() = default;
CardMetadataLoggingContext::CardMetadataLoggingContext(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext::CardMetadataLoggingContext(
    CardMetadataLoggingContext&&) = default;
CardMetadataLoggingContext& CardMetadataLoggingContext::operator=(
    const CardMetadataLoggingContext&) = default;
CardMetadataLoggingContext& CardMetadataLoggingContext::operator=(
    CardMetadataLoggingContext&&) = default;
CardMetadataLoggingContext::~CardMetadataLoggingContext() = default;

bool CardMetadataLoggingContext::DidShowCardWithBenefitAvailable() const {
  return !instrument_ids_to_issuer_ids_with_benefits_available.empty();
}

bool CardMetadataLoggingContext::SelectedCardHasBenefitAvailable() const {
  return instrument_ids_to_issuer_ids_with_benefits_available.contains(
      selected_card_instrument_id);
}

bool CardMetadataLoggingContext::SelectedCardHasMetadataAvailable() const {
  return instruments_with_metadata_available.contains(
      selected_card_instrument_id);
}

void CardMetadataLoggingContext::SetSelectedCardInfo(
    const CreditCard& credit_card) {
  selected_card_instrument_id = credit_card.instrument_id();
  selected_issuer_id = credit_card.issuer_id();

  selected_issuer_or_network_to_metadata_availability = {
      {selected_issuer_id, SelectedCardHasMetadataAvailable()},
      {credit_card.network(), SelectedCardHasMetadataAvailable()}};
}

std::string_view GetCardIssuerIdOrNetworkSuffix(
    const std::string& card_issuer_id_or_network) {
  if (card_issuer_id_or_network == kAmexCardIssuerId) {
    return kAmericanExpress;
  } else if (card_issuer_id_or_network == kAnzCardIssuerId) {
    return kAnz;
  } else if (card_issuer_id_or_network == kCapitalOneCardIssuerId) {
    return kCapitalOne;
  } else if (card_issuer_id_or_network == kChaseCardIssuerId) {
    return kChase;
  } else if (card_issuer_id_or_network == kCitiCardIssuerId) {
    return kCiti;
  } else if (card_issuer_id_or_network == kDiscoverCardIssuerId) {
    return kDiscover;
  } else if (card_issuer_id_or_network == kLloydsCardIssuerId) {
    return kLloyds;
  } else if (card_issuer_id_or_network == kMarqetaCardIssuerId) {
    return kMarqeta;
  } else if (card_issuer_id_or_network == kNabCardIssuerId) {
    return kNab;
  } else if (card_issuer_id_or_network == kNatwestCardIssuerId) {
    return kNatwest;
  } else if (card_issuer_id_or_network == autofill::kMasterCard) {
    return kMastercard;
  } else if (card_issuer_id_or_network == autofill::kVisaCard) {
    return kVisa;
  } else {
    return "";
  }
}

CardMetadataLoggingContext GetMetadataLoggingContext(
    const std::vector<CreditCard>& cards) {
  const base::flat_set<std::string> kLoggedNetworks{autofill::kMasterCard,
                                                    autofill::kVisaCard};
  CardMetadataLoggingContext metadata_logging_context;
  for (const CreditCard& card : cards) {
    // If there is a product description, denote in the
    // `metadata_logging_context` that we have shown at least one product
    // description so we can log it later.
    if (!card.product_description().empty()) {
      metadata_logging_context.card_product_description_shown =
          base::FeatureList::IsEnabled(
              features::kAutofillEnableCardProductName);
    }

    // If there is rich card art we received from the metadata for this card,
    // denote in the `metadata_logging_context` that we have shown an enriched
    // card art so we can log it later.
    if (card.HasRichCardArtImageFromMetadata()) {
      metadata_logging_context.card_art_image_shown =
          base::FeatureList::IsEnabled(features::kAutofillEnableCardArtImage);
    }

    bool card_has_metadata = !card.product_description().empty() ||
                             card.HasRichCardArtImageFromMetadata();

    if (!card.issuer_id().empty()) {
      metadata_logging_context
          .issuer_or_network_to_metadata_availability[card.issuer_id()] |=
          card_has_metadata;
    }
    if (kLoggedNetworks.contains(card.network())) {
      metadata_logging_context
          .issuer_or_network_to_metadata_availability[card.network()] |=
          card_has_metadata;
    }

    if (card_has_metadata) {
      metadata_logging_context.instruments_with_metadata_available.insert(
          card.instrument_id());
    }
  }

  return metadata_logging_context;
}

// TODO(crbug.com/41494039): Refactor and cleanup FormEvent logging.
void LogCardWithMetadataFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context,
    HasBeenLogged has_been_logged) {
  bool selected_with_metadata_logged = false;
  for (const auto& [issuer_or_network, has_metadata] :
       context.selected_issuer_or_network_to_metadata_availability.has_value()
           ? *context.selected_issuer_or_network_to_metadata_availability
           : context.issuer_or_network_to_metadata_availability) {
    const std::string_view& histogram_issuer_or_network =
        GetCardIssuerIdOrNetworkSuffix(issuer_or_network);
    if (histogram_issuer_or_network.empty()) {
      continue;
    }

    switch (event) {
      case CardMetadataLoggingEvent::kShown:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                          ".ShownWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(
              base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                            ".ShownWithMetadataOnce"}),
              has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kSelected:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                          ".SelectedWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(
              base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                            ".SelectedWithMetadataOnce"}),
              has_metadata);
          if (has_metadata) {
            base::UmaHistogramBoolean(
                base::StrCat({"Autofill.CreditCard.",
                              histogram_issuer_or_network,
                              ".SelectedWithIssuerMetadataPresentOnce"}),
                true);
          }
          if (!selected_with_metadata_logged) {
            // Only log not selected cards once per selected event.
            selected_with_metadata_logged = true;

            // Log which issuers and networks with metadata were not selected.
            for (const std::string& not_selected_issuer_ids_and_networks :
                 context.not_selected_issuer_ids_and_networks) {
              base::UmaHistogramBoolean(
                  base::StrCat({"Autofill.CreditCard.",
                                GetCardIssuerIdOrNetworkSuffix(
                                    not_selected_issuer_ids_and_networks),
                                ".SelectedWithIssuerMetadataPresentOnce"}),
                  false);
            }
          }
        }
        break;
      case CardMetadataLoggingEvent::kFilled:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                          ".FilledWithMetadata"}),
            has_metadata);
        if (!has_been_logged.value()) {
          base::UmaHistogramBoolean(
              base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                            ".FilledWithMetadataOnce"}),
              has_metadata);
        }
        break;
      case CardMetadataLoggingEvent::kWillSubmit:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                          ".WillSubmitWithMetadataOnce"}),
            has_metadata);
        break;
      case CardMetadataLoggingEvent::kSubmitted:
        base::UmaHistogramBoolean(
            base::StrCat({"Autofill.CreditCard.", histogram_issuer_or_network,
                          ".SubmittedWithMetadataOnce"}),
            has_metadata);
        break;
    }
  }
}

void LogCardWithBenefitFormEventMetric(
    CardMetadataLoggingEvent event,
    const CardMetadataLoggingContext& context) {
  switch (event) {
    case CardMetadataLoggingEvent::kShown: {
      LogBenefitFormEventForAllIssuersWithBenefitAvailable(
          context.instrument_ids_to_issuer_ids_with_benefits_available,
          FORM_EVENT_SUGGESTION_FOR_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE);
      break;
    }
    case CardMetadataLoggingEvent::kSelected:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToIssuerHistogram(
            context.selected_issuer_id,
            FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SELECTED_ONCE);
      }
      LogBenefitFormEventForAllIssuersWithBenefitAvailable(
          context.instrument_ids_to_issuer_ids_with_benefits_available,
          FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SELECTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE);
      break;
    case CardMetadataLoggingEvent::kFilled:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToIssuerHistogram(
            context.selected_issuer_id,
            FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_FILLED_ONCE);
      }
      LogBenefitFormEventForAllIssuersWithBenefitAvailable(
          context.instrument_ids_to_issuer_ids_with_benefits_available,
          FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_FILLED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE);
      break;
    case CardMetadataLoggingEvent::kSubmitted:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToIssuerHistogram(
            context.selected_issuer_id,
            FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_WITH_BENEFIT_AVAILABLE_SUBMITTED_ONCE);
      }
      LogBenefitFormEventForAllIssuersWithBenefitAvailable(
          context.instrument_ids_to_issuer_ids_with_benefits_available,
          FORM_EVENT_SUGGESTION_FOR_SERVER_CARD_SUBMITTED_AFTER_CARD_WITH_BENEFIT_AVAILABLE_SHOWN_ONCE);
      break;
    case CardMetadataLoggingEvent::kWillSubmit:
      // Currently do not log kWillSubmit events for benefits.
      break;
  }
}

void LogAcceptanceLatency(base::TimeDelta latency,
                          const CardMetadataLoggingContext& suggestion_context,
                          const CreditCard& selected_card) {
  std::string histogram_name_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown.";
  base::UmaHistogramMediumTimes(
      base::StrCat({histogram_name_prefix,
                    GetMetadataAvailabilitySuffix(suggestion_context)}),
      latency);

  for (const std::string& issuer_or_network :
       {selected_card.issuer_id(), selected_card.network()}) {
    const std::string_view& issuer_or_network_suffix =
        GetCardIssuerIdOrNetworkSuffix(issuer_or_network);
    if (issuer_or_network_suffix.empty()) {
      continue;
    }
    base::UmaHistogramMediumTimes(
        base::StrCat({histogram_name_prefix, "CardWithIssuerId.",
                      GetMetadataAvailabilitySuffix(GetMetadataLoggingContext(
                          std::vector<CreditCard>{selected_card})),
                      ".", issuer_or_network_suffix}),
        latency);
  }
}

void LogIsCreditCardBenefitsEnabledAtStartup(bool enabled) {
  base::UmaHistogramBoolean(
      "Autofill.PaymentMethods.CardBenefitsIsEnabled.Startup", enabled);
}

void LogBenefitFormEventToIssuerHistogram(const std::string& issuer_id,
                                          FormEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Autofill.FormEvents.CreditCard."
                    "WithBenefits.",
                    GetCardIssuerIdOrNetworkSuffix(issuer_id)}),
      event, NUM_FORM_EVENTS);
}

void LogBenefitFormEventForAllIssuersWithBenefitAvailable(
    const base::flat_map<int64_t, std::string>&
        instrument_ids_to_issuer_ids_with_benefits_available,
    FormEvent event) {
  // `issuers_shown` holds all credit card issuers that were shown with
  // benefits available to the user and logged for the `event`.
  std::unordered_set<std::string> issuers_shown;

  for (const auto& [instrument_id, issuer_id] :
       instrument_ids_to_issuer_ids_with_benefits_available) {
    if (!issuers_shown.contains(issuer_id)) {
      LogBenefitFormEventToIssuerHistogram(issuer_id, event);
      issuers_shown.insert(issuer_id);
    }
  }
}

}  // namespace autofill::autofill_metrics
