// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/plus_address_test_utils.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/plus_addresses/grit/plus_addresses_strings.h"
#include "components/plus_addresses/plus_address_preallocator.h"
#include "components/plus_addresses/plus_address_types.h"
#include "ui/base/l10n/l10n_util.h"

namespace plus_addresses::test {
namespace {
using autofill::EqualsSuggestion;
using autofill::Suggestion;
using autofill::SuggestionType;
using ::testing::Field;
using ::testing::Matcher;
}  // namespace

PlusProfile CreatePlusProfile(std::string plus_address, bool is_confirmed) {
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromCanonicalSpec("https://foo.com");
  return PlusProfile(/*profile_id=*/"123", facet,
                     PlusAddress(std::move(plus_address)),
                     /*is_confirmed=*/is_confirmed);
}

PlusProfile CreatePlusProfile() {
  return CreatePlusProfile("plus+foo@plus.plus", /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfile2() {
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromCanonicalSpec("https://bar.com");
  return PlusProfile(/*profile_id=*/"234", facet,
                     PlusAddress("plus+bar@plus.plus"),
                     /*is_confirmed=*/true);
}

PlusProfile CreatePlusProfileWithFacet(const affiliations::FacetURI& facet) {
  PlusProfile profile = CreatePlusProfile();
  profile.facet = facet;
  return profile;
}

std::string MakeCreationResponse(const PlusProfile& profile) {
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "plusProfile": $1
          }
        )",
      {MakePlusProfile(profile)}, nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

std::string MakeListResponse(const std::vector<PlusProfile>& profiles) {
  base::Value::Dict response;
  base::Value::List list;
  for (const PlusProfile& profile : profiles) {
    std::string json = MakePlusProfile(profile);
    std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(json);
    DCHECK(dict.has_value());
    list.Append(std::move(dict.value()));
  }
  response.Set("plusProfiles", std::move(list));
  std::optional<std::string> json = base::WriteJson(response);
  DCHECK(json.has_value());
  return json.value();
}

std::string MakePreallocateResponse(
    const std::vector<PreallocatedPlusAddress>& addresses) {
  base::Value::List profiles;
  for (const PreallocatedPlusAddress& address : addresses) {
    profiles.Append(
        base::Value::Dict()
            .Set("emailAddress", *address.plus_address)
            .Set("reservationLifetime",
                 base::NumberToString(address.lifetime.InSeconds()) + "s"));
  }
  return base::WriteJson(
             base::Value::Dict().Set("emailAddresses", std::move(profiles)))
      .value();
}

std::string MakePlusProfile(const PlusProfile& profile) {
  // Note: the below must be kept in-line with the PlusAddressParser behavior.
  std::string mode = profile.is_confirmed ? "anyMode" : "UNSPECIFIED";
  std::string json = base::ReplaceStringPlaceholders(
      R"(
          {
            "ProfileId": "$1",
            "facet": "$2",
            "plusEmail": {
              "plusAddress": "$3",
              "plusMode": "$4"
            }
          }
        )",
      {*profile.profile_id, profile.facet.canonical_spec(),
       *profile.plus_address, mode},
      nullptr);
  DCHECK(base::JSONReader::Read(json));
  return json;
}

std::unique_ptr<net::test_server::HttpResponse>
HandleRequestToPlusAddressWithSuccess(
    const net::test_server::HttpRequest& request) {
  // Ignore unrecognized path.
  if (request.GetURL().path() != kReservePath &&
      request.GetURL().path() != kConfirmPath) {
    return nullptr;
  }

  bool is_refresh = [&]() {
    std::optional<base::Value::Dict> body =
        base::JSONReader::ReadDict(request.content);
    if (!body) {
      return false;
    }
    return body->FindBool("refresh_email_address").value_or(false);
  }();
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("application/json");
  PlusProfile profile = CreatePlusProfile(
      /*plus_address=*/is_refresh ? kFakePlusAddressRefresh : kFakePlusAddress,
      /*is_confirmed=*/request.GetURL().path() == kConfirmPath);
  http_response->set_content(MakeCreationResponse(profile));
  return http_response;
}

base::Value CreatePreallocatedPlusAddress(base::Time end_of_life,
                                          std::string address) {
  return base::Value(
      base::Value::Dict()
          .Set(PlusAddressPreallocator::kEndOfLifeKey,
               base::TimeToValue(end_of_life))
          .Set(PlusAddressPreallocator::kPlusAddressKey, std::move(address)));
}

Matcher<Suggestion> EqualsFillPlusAddressSuggestion(std::string_view address) {
  std::vector<std::vector<Suggestion::Text>> labels;
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_FILL_SUGGESTION_SECONDARY_TEXT))}};
  }
  return AllOf(EqualsSuggestion(SuggestionType::kFillExistingPlusAddress,
                                /*main_text=*/base::UTF8ToUTF16(address)),
               Field(&Suggestion::icon, Suggestion::Icon::kPlusAddress),
               Field(&Suggestion::labels, labels));
}

Matcher<std::vector<Suggestion>> IsSingleCreatePlusAddressSuggestion() {
  std::vector<std::vector<Suggestion::Text>> labels;
  if constexpr (!BUILDFLAG(IS_ANDROID)) {
    labels = {{Suggestion::Text(l10n_util::GetStringUTF16(
        IDS_PLUS_ADDRESS_CREATE_SUGGESTION_SECONDARY_TEXT))}};
  }
  return ElementsAre(AllOf(
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      EqualsSuggestion(SuggestionType::kCreateNewPlusAddressInline),
#else
      EqualsSuggestion(SuggestionType::kCreateNewPlusAddress,
                       /*main_text=*/l10n_util::GetStringUTF16(
                           IDS_PLUS_ADDRESS_CREATE_SUGGESTION_MAIN_TEXT)),
      Field(&Suggestion::iph_metadata,
            Suggestion::IPHMetadata(
                &feature_engagement::kIPHPlusAddressCreateSuggestionFeature)),
#endif
      Field(&Suggestion::icon, Suggestion::Icon::kPlusAddress),
      Field(&Suggestion::labels, labels)));
}

Matcher<std::vector<Suggestion>> IsSingleFillPlusAddressSuggestion(
    std::string_view address) {
  return ElementsAre(EqualsFillPlusAddressSuggestion(address));
}

}  // namespace plus_addresses::test
