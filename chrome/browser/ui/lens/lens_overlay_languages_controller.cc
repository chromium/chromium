// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_languages_controller.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lens/core/mojom/translate.mojom-forward.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/common/channel_info.h"
#include "google_apis/common/api_key_request_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "ui/base/l10n/l10n_util.h"

namespace lens {

namespace {

constexpr int kMaxDownloadBytes = 1024 * 1024;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("lens_overlay_languages", R"(
        semantics {
          sender: "Lens"
          description: "A request to the translate GetSupportedLanguages "
            "endpoint for the Lens Overlay feature in Chrome."
          trigger: "The user triggered a Lens Overlay Flow by entering "
            "the experience via an entrypoint such as the Lens icon in the "
            "toolbar or Lens context menu item. This annotation corresponds "
            "to the translate GetSupportedLanguages requests sent by the Lens "
            "overlay to get language names and codes to show in the language "
            "pickers of the full page translate feature. This response is "
            "cached in the WebUI local storage and as such the request will "
            "only be made when certain conditions are met to invalidate the "
            "cache. This can include when the browser locale was changed or "
            "data is stale by more than a defined time period (e.g. a month)."
          data: "Only the browser process's locale separated into a language "
            "code and country code are used in the request. These are used to "
            "get supported languages with display names appropriate to the "
            "user's locale."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "juanmojica@google.com"
            }
            contacts {
              email: "lens-chrome@google.com"
            }
          }
          user_data {
            type: NONE
          }
          last_reviewed: "2024-10-24"
        }
        policy {
          cookies_allowed: NO
          cookies_store: ""
          setting: "This feature is only shown in menus by default and does "
            "nothing without explicit user action."
          chrome_policy {
            LensOverlaySettings {
              LensOverlaySettings: 1
            }
          }
        }
      )");
}  // namespace

LensOverlayLanguagesController::LensOverlayLanguagesController(Profile* profile)
    : profile_(profile) {}
LensOverlayLanguagesController::~LensOverlayLanguagesController() = default;

void LensOverlayLanguagesController::SendGetSupportedLanguagesRequest(
    SupportedLanguagesRetrievedCallback callback) {
  callback_ = std::move(callback);
  url_loader_ = InitializeURLLoader();
  url_loader_->DownloadToString(
      profile_->GetURLLoaderFactory().get(),
      base::BindOnce(
          &LensOverlayLanguagesController::OnGetSupportedLanguagesResponse,
          weak_ptr_factory_.GetWeakPtr()),
      kMaxDownloadBytes);
}

void LensOverlayLanguagesController::OnGetSupportedLanguagesResponse(
    std::optional<std::string> response_body) {
  if (!response_body) {
    std::move(callback_).Run(locale_, std::vector<mojom::LanguagePtr>(),
                             std::vector<mojom::LanguagePtr>());
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response_body.value(),
      base::BindOnce(&LensOverlayLanguagesController::OnJsonParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LensOverlayLanguagesController::OnJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (!result.has_value()) {
    std::move(callback_).Run(locale_, std::vector<mojom::LanguagePtr>(),
                             std::vector<mojom::LanguagePtr>());
    return;
  }

  if (!result->is_dict()) {
    std::move(callback_).Run(locale_, std::vector<mojom::LanguagePtr>(),
                             std::vector<mojom::LanguagePtr>());
    return;
  }
  const base::Value::Dict& result_dict = result->GetDict();
  const base::Value::List* source_language_list =
      result_dict.FindList("sourceLanguages");
  const base::Value::List* target_language_list =
      result_dict.FindList("targetLanguages");

  std::vector<lens::mojom::LanguagePtr> source_languages =
      RetrieveLanguagesFromResults(source_language_list);
  std::vector<lens::mojom::LanguagePtr> target_languages =
      RetrieveLanguagesFromResults(target_language_list);

  if (callback_) {
    std::move(callback_).Run(locale_, std::move(source_languages),
                             std::move(target_languages));
  }
}

std::vector<lens::mojom::LanguagePtr>
LensOverlayLanguagesController::RetrieveLanguagesFromResults(
    const base::Value::List* result_list) {
  if (!result_list) {
    return std::vector<lens::mojom::LanguagePtr>();
  }

  std::vector<lens::mojom::LanguagePtr> languages_list;
  for (const base::Value& result_language : *result_list) {
    if (!result_language.is_dict()) {
      continue;
    }

    const base::Value::Dict& result_language_dict = result_language.GetDict();
    const std::string* language_code =
        result_language_dict.FindString("language");
    const std::string* language_name = result_language_dict.FindString("name");
    if (language_code && language_name) {
      lens::mojom::LanguagePtr language = lens::mojom::Language::New();
      language->language_code = language_code->c_str();
      language->name = language_name->c_str();
      languages_list.push_back(std::move(language));
    }
  }
  return languages_list;
}

std::unique_ptr<network::SimpleURLLoader>
LensOverlayLanguagesController::InitializeURLLoader() {
  std::unique_ptr<network::ResourceRequest> resource_request =
      std::make_unique<network::ResourceRequest>();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  locale_ = g_browser_process->GetApplicationLocale();
  const auto country = l10n_util::GetCountry(locale_);
  const auto language = l10n_util::GetLanguage(locale_);
  resource_request->url = BuildTranslateLanguagesURL(country, language);

  google_apis::AddDefaultAPIKeyToRequest(*resource_request,
                                         chrome::GetChannel());
  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kTrafficAnnotationTag);
}

}  // namespace lens
