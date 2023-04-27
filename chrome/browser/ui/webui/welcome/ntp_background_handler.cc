// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/ntp_background_handler.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_backgrounds.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/background/ntp_custom_background_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/welcome_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace welcome {

enum class NtpBackgrounds {
  kArt = 0,
  kCityscape = 1,
  kEarth = 2,
  kGeometricShapes = 3,
  kLandscape = 4,
};

NtpBackgroundHandler::NtpBackgroundHandler() {}
NtpBackgroundHandler::~NtpBackgroundHandler() {}

void NtpBackgroundHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "clearBackground",
      base::BindRepeating(&NtpBackgroundHandler::HandleClearBackground,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getBackgrounds",
      base::BindRepeating(&NtpBackgroundHandler::HandleGetBackgrounds,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "setBackground",
      base::BindRepeating(&NtpBackgroundHandler::HandleSetBackground,
                          base::Unretained(this)));
}

void NtpBackgroundHandler::HandleClearBackground(
    const base::Value::List& args) {
  auto* service = NtpCustomBackgroundServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  service->ResetCustomBackgroundInfo();
}

void NtpBackgroundHandler::HandleGetBackgrounds(const base::Value::List& args) {
  AllowJavascript();
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];

  base::Value::List list_value;
  std::array<GURL, kNtpBackgroundsCount> NtpBackgrounds = GetNtpBackgrounds();
  const std::string kUrlPrefix = "preview-background.jpg?";

  {
    base::Value::Dict element;
    int id = static_cast<int>(NtpBackgrounds::kEarth);
    element.Set("id", id);
    element.Set("title", l10n_util::GetStringUTF8(
                             IDS_WELCOME_NTP_BACKGROUND_EARTH_TITLE));
    element.Set("imageUrl", kUrlPrefix + base::NumberToString(id));
    element.Set("thumbnailClass", "earth");
    list_value.Append(std::move(element));
  }
  {
    base::Value::Dict element;
    int id = static_cast<int>(NtpBackgrounds::kCityscape);
    element.Set("id", id);
    element.Set("title", l10n_util::GetStringUTF8(
                             IDS_WELCOME_NTP_BACKGROUND_CITYSCAPE_TITLE));
    element.Set("imageUrl", kUrlPrefix + base::NumberToString(id));
    element.Set("thumbnailClass", "cityscape");
    list_value.Append(std::move(element));
  }
  {
    base::Value::Dict element;
    int id = static_cast<int>(NtpBackgrounds::kLandscape);
    element.Set("id", id);
    element.Set("title", l10n_util::GetStringUTF8(
                             IDS_WELCOME_NTP_BACKGROUND_LANDSCAPE_TITLE));
    element.Set("imageUrl", kUrlPrefix + base::NumberToString(id));
    element.Set("thumbnailClass", "landscape");
    list_value.Append(std::move(element));
  }
  {
    base::Value::Dict element;
    int id = static_cast<int>(NtpBackgrounds::kArt);
    element.Set("id", id);
    element.Set("title",
                l10n_util::GetStringUTF8(IDS_WELCOME_NTP_BACKGROUND_ART_TITLE));
    element.Set("imageUrl", kUrlPrefix + base::NumberToString(id));
    element.Set("thumbnailClass", "art");
    list_value.Append(std::move(element));
  }
  {
    base::Value::Dict element;
    int id = static_cast<int>(NtpBackgrounds::kGeometricShapes);
    element.Set("id", id);
    element.Set("title",
                l10n_util::GetStringUTF8(
                    IDS_WELCOME_NTP_BACKGROUND_GEOMETRIC_SHAPES_TITLE));
    element.Set("imageUrl", kUrlPrefix + base::NumberToString(id));
    element.Set("thumbnailClass", "geometric-shapes");
    list_value.Append(std::move(element));
  }

  ResolveJavascriptCallback(callback_id, list_value);
}

void NtpBackgroundHandler::HandleSetBackground(const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  int background_index = args[0].GetInt();

  std::array<GURL, kNtpBackgroundsCount> NtpBackgrounds = GetNtpBackgrounds();
  auto* service = NtpCustomBackgroundServiceFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));

  switch (background_index) {
    case static_cast<int>(NtpBackgrounds::kArt):
      service->SetCustomBackgroundInfo(NtpBackgrounds[background_index], GURL(),
                                       "Universe Cosmic Vacum",
                                       "Philipp Rietz — Walli", GURL(), "");
      break;
    case static_cast<int>(NtpBackgrounds::kCityscape):
      service->SetCustomBackgroundInfo(
          NtpBackgrounds[background_index], GURL(),
          l10n_util::GetStringFUTF8(IDS_WELCOME_NTP_BACKGROUND_PHOTO_BY_LABEL,
                                    u"Ev Tchebotarev"),
          "",
          GURL("https://500px.com/photo/135751035/"
               "soulseek-by-%E5%B0%A4%E9%87%91%E5%B0%BC-ev-tchebotarev"),
          "");
      break;
    case static_cast<int>(NtpBackgrounds::kEarth):
      service->SetCustomBackgroundInfo(
          NtpBackgrounds[background_index], GURL(),
          l10n_util::GetStringFUTF8(IDS_WELCOME_NTP_BACKGROUND_PHOTO_BY_LABEL,
                                    u"NASA Image Library"),
          "", GURL("https://www.google.com/sky/"), "");
      break;
    case static_cast<int>(NtpBackgrounds::kGeometricShapes):
      service->SetCustomBackgroundInfo(NtpBackgrounds[background_index], GURL(),
                                       "Tessellation 15", "Justin Prno — Walli",
                                       GURL(), "");
      break;
    case static_cast<int>(NtpBackgrounds::kLandscape):
      service->SetCustomBackgroundInfo(
          NtpBackgrounds[background_index], GURL(),
          l10n_util::GetStringFUTF8(IDS_WELCOME_NTP_BACKGROUND_PHOTO_BY_LABEL,
                                    u"Giulio Rosso Chioso"),
          "",
          GURL("https://500px.com/photo/41149196/"
               "le-piscine-sunset-by-giulio-rosso-chioso"),
          "");
      break;
  }
}

}  // namespace welcome
