// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"

#include <unordered_set>

#include "base/containers/fixed_flat_set.h"
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

void LogBenefitFormEventToMainBenefitHistogram(CardBenefitFormEvent event) {
  base::UmaHistogramEnumeration("Autofill.FormEvents.CreditCard.Benefits",
                                event);
}

void LogBenefitFormEventToBenefitSubhistogram(std::string_view benefit_source,
                                              CardBenefitFormEvent event) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {"Autofill.FormEvents.CreditCard.Benefits.", benefit_source}),
      event);
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
  return !instrument_ids_to_available_benefit_sources.empty();
}

bool CardMetadataLoggingContext::SelectedCardHasBenefitAvailable() const {
  return instrument_ids_to_available_benefit_sources.contains(
      selected_card_instrument_id);
}

bool CardMetadataLoggingContext::SelectedCardHasMetadataAvailable() const {
  return instruments_with_metadata_available.contains(
      selected_card_instrument_id);
}

void CardMetadataLoggingContext::SetSelectedCardInfo(
    const CreditCard& credit_card) {
  selected_card_instrument_id = credit_card.instrument_id();
  selected_benefit_source = credit_card.benefit_source();

  selected_issuer_or_network_to_metadata_availability = {
      {credit_card.issuer_id(), SelectedCardHasMetadataAvailable()},
      {credit_card.network(), SelectedCardHasMetadataAvailable()}};
}

std::string_view GetCardIssuerIdOrNetworkSuffix(
    std::string_view card_issuer_id_or_network) {
  if (card_issuer_id_or_network == kAmexCardIssuerId) {
    return kAmericanExpress;
  } else if (card_issuer_id_or_network == kAnzCardIssuerId) {
    return kAnz;
  } else if (card_issuer_id_or_network == kBmoCardIssuerId) {
    return kBmo;
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
  } else if (card_issuer_id_or_network == kMasterCard) {
    return kMastercard;
  } else if (card_issuer_id_or_network == kVisaCard) {
    return kVisa;
  } else {
    return "";
  }
}

std::string_view GetCardBenefitSourceSuffix(
    std::string_view card_benefit_source) {
  if (card_benefit_source == kAmexCardBenefitSource) {
    return kAmericanExpress;
  } else if (card_benefit_source == kBmoCardBenefitSource) {
    return kBmo;
  } else if (card_benefit_source == kCurinosCardBenefitSource) {
    return kCurinos;
  } else {
    return "";
  }
}

CardMetadataLoggingContext GetMetadataLoggingContext(
    base::span<const CreditCard> cards) {
  constexpr auto kLoggedNetworks =
      base::MakeFixedFlatSet<std::string_view>({kMasterCard, kVisaCard});
  CardMetadataLoggingContext metadata_logging_context;
  for (const CreditCard& card : cards) {
    // If there is a product description, denote in the
    // `metadata_logging_context` that we have shown at least one product
    // description so we can log it later.
    if (!card.product_description().empty()) {
      metadata_logging_context.card_product_description_shown = true;
    }

    // If there is rich card art we received from the metadata for this card,
    // denote in the `metadata_logging_context` that we have shown an enriched
    // card art so we can log it later.
    if (card.HasRichCardArtImageFromMetadata()) {
      metadata_logging_context.card_art_image_shown = true;
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

    if (card.record_type() ==
        autofill::CreditCard::RecordType::kMaskedServerCard) {
      metadata_logging_context.masked_server_card_count++;
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

void LogCardBenefitFormEventMetrics(CardMetadataLoggingEvent event,
                                    const CardMetadataLoggingContext& context) {
  switch (event) {
    case CardMetadataLoggingEvent::kShown: {
      LogBenefitFormEventToAllBenefitHistograms(
          context.instrument_ids_to_available_benefit_sources,
          CardBenefitFormEvent::kSuggestionWithBenefitShown);
      if (context.masked_server_card_count >= 2) {
        LogBenefitFormEventToAllBenefitHistograms(
            context.instrument_ids_to_available_benefit_sources,
            CardBenefitFormEvent::
                kSuggestionWithBenefitShownWithMultipleServerCards);
      }
      break;
    }
    case CardMetadataLoggingEvent::kSelected:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToAllBenefitHistograms(
            context.selected_benefit_source,
            CardBenefitFormEvent::kSuggestionWithBenefitSelected);
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToAllBenefitHistograms(
              context.selected_benefit_source,
              CardBenefitFormEvent::
                  kSuggestionWithBenefitSelectedWithMultipleServerCards);
        }
      } else {
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToMainBenefitHistogram(
              CardBenefitFormEvent::
                  kSuggestionWithoutBenefitSelectedWithMultipleServerCards);
        }
      }
      break;
    case CardMetadataLoggingEvent::kFilled:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToAllBenefitHistograms(
            context.selected_benefit_source,
            CardBenefitFormEvent::kSuggestionWithBenefitFilled);
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToAllBenefitHistograms(
              context.selected_benefit_source,
              CardBenefitFormEvent::
                  kSuggestionWithBenefitFilledWithMultipleServerCards);
        }
      } else {
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToMainBenefitHistogram(
              CardBenefitFormEvent::
                  kSuggestionWithoutBenefitFilledWithMultipleServerCards);
        }
      }
      break;
    case CardMetadataLoggingEvent::kSubmitted:
      if (context.SelectedCardHasBenefitAvailable()) {
        LogBenefitFormEventToAllBenefitHistograms(
            context.selected_benefit_source,
            CardBenefitFormEvent::kSuggestionWithBenefitSubmitted);
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToAllBenefitHistograms(
              context.selected_benefit_source,
              CardBenefitFormEvent::
                  kSuggestionWithBenefitSubmittedWithMultipleServerCards);
        }
      } else {
        if (context.masked_server_card_count >= 2) {
          LogBenefitFormEventToMainBenefitHistogram(
              CardBenefitFormEvent::
                  kSuggestionWithoutBenefitSubmittedWithMultipleServerCards);
        }
      }
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

void LogBenefitFormEventToAllBenefitHistograms(
    const base::flat_map<int64_t, std::string>&
        instrument_ids_to_available_benefit_sources,
    CardBenefitFormEvent event) {
  // `benefit_sources_logged` holds all credit card benefit sources that were
  // shown with benefits available to the user and logged for the `event`.
  std::unordered_set<std::string_view> benefit_sources_logged;
  for (const auto& [instrument_id, benefit_source] :
       instrument_ids_to_available_benefit_sources) {
    std::string_view benefit_source_suffix =
        GetCardBenefitSourceSuffix(benefit_source);
    if (benefit_source_suffix.empty()) {
      continue;
    }
    if (!benefit_sources_logged.contains(benefit_source_suffix)) {
      LogBenefitFormEventToBenefitSubhistogram(benefit_source_suffix, event);
      benefit_sources_logged.insert(benefit_source_suffix);
    }
  }
  // Only log to the main benefit histogram if a valid benefit source was logged
  // to the benefit subhistogram.
  if (!benefit_sources_logged.empty()) {
    LogBenefitFormEventToMainBenefitHistogram(event);
  }
}

void LogBenefitFormEventToAllBenefitHistograms(std::string_view benefit_source,
                                               CardBenefitFormEvent event) {
  std::string_view benefit_source_suffix =
      GetCardBenefitSourceSuffix(benefit_source);
  if (benefit_source_suffix.empty()) {
    return;
  }
  LogBenefitFormEventToMainBenefitHistogram(event);
  LogBenefitFormEventToBenefitSubhistogram(benefit_source_suffix, event);
}

}  // namespace autofill::autofill_metrics
