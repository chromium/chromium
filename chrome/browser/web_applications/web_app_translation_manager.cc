// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_translation_manager.h"

#include "base/containers/span.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/web_applications/proto/web_app_translations.pb.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "third_party/blink/public/common/features.h"

namespace web_app {

namespace {

constexpr base::TaskTraits kTaskTraits = {
    base::MayBlock(), base::TaskPriority::USER_VISIBLE,
    base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

base::FilePath GetDirectory(const base::FilePath& web_apps_directory) {
  return web_apps_directory.AppendASCII("Translations");
}

LocaleOverrides ConvertTranslationItemToLocaleOverrides(
    blink::Manifest::TranslationItem translation) {
  LocaleOverrides locale_overrides;
  if (translation.name) {
    locale_overrides.set_name(translation.name.value());
  }
  if (translation.short_name) {
    locale_overrides.set_short_name(translation.short_name.value());
  }
  if (translation.description) {
    locale_overrides.set_description(translation.description.value());
  }
  return locale_overrides;
}

blink::Manifest::TranslationItem ConvertLocaleOverridesToTranslationItem(
    const LocaleOverrides& locale_overrides) {
  blink::Manifest::TranslationItem translation_item;

  if (locale_overrides.has_name()) {
    translation_item.name = locale_overrides.name();
  }

  if (locale_overrides.has_short_name()) {
    translation_item.short_name = locale_overrides.short_name();
  }

  if (locale_overrides.has_description()) {
    translation_item.description = locale_overrides.description();
  }

  return translation_item;
}

AllTranslations ReadProtoBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                  const base::FilePath& web_apps_directory) {
  base::FilePath translations_dir = GetDirectory(web_apps_directory);
  std::string value;
  if (!utils->ReadFileToString(translations_dir, &value)) {
    return AllTranslations();
  }
  AllTranslations proto;
  if (!proto.ParseFromString(value)) {
    return AllTranslations();
  }
  return proto;
}

bool WriteProtoBlocking(scoped_refptr<FileUtilsWrapper> utils,
                        const base::FilePath& web_apps_directory,
                        AllTranslations proto) {
  base::FilePath translations_dir = GetDirectory(web_apps_directory);
  std::string proto_as_string = proto.SerializeAsString();
  return utils->WriteFile(translations_dir,
                          base::as_byte_span(proto_as_string));
}

bool DeleteTranslationsBlocking(scoped_refptr<FileUtilsWrapper> utils,
                                const base::FilePath& web_apps_directory,
                                const webapps::AppId& app_id) {
  if (!utils->CreateDirectory(web_apps_directory)) {
    return false;
  }
  AllTranslations proto = ReadProtoBlocking(utils, web_apps_directory);

  proto.mutable_id_to_translations_map()->erase(app_id);

  return WriteProtoBlocking(utils, web_apps_directory, std::move(proto));
}

bool WriteTranslationsBlocking(
    scoped_refptr<FileUtilsWrapper> utils,
    const base::FilePath& web_apps_directory,
    const webapps::AppId& app_id,
    base::flat_map<Locale, blink::Manifest::TranslationItem> translations) {
  if (!utils->CreateDirectory(web_apps_directory)) {
    return false;
  }

  AllTranslations proto = ReadProtoBlocking(utils, web_apps_directory);

  auto* mutable_id_to_translations_map = proto.mutable_id_to_translations_map();
  mutable_id_to_translations_map->erase(app_id);

  WebAppTranslations locale_to_overrides_map;

  for (const auto& translation : translations) {
    (*locale_to_overrides_map
          .mutable_locale_to_overrides_map())[translation.first] =
        ConvertTranslationItemToLocaleOverrides(translation.second);
  }
  (*proto.mutable_id_to_translations_map())[app_id] = locale_to_overrides_map;

  return WriteProtoBlocking(utils, web_apps_directory, proto);
}

}  // namespace

WebAppTranslationManager::WebAppTranslationManager(Profile* profile) {
  web_apps_directory_ = GetWebAppsRootDirectory(profile);
}

WebAppTranslationManager::~WebAppTranslationManager() = default;

void WebAppTranslationManager::SetProvider(base::PassKey<WebAppProvider>,
                                           WebAppProvider& provider) {
  provider_ = &provider;
}

void WebAppTranslationManager::Start() {
  if (base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    ReadTranslations(base::DoNothing());
  }
}

void WebAppTranslationManager::WriteTranslations(
    const webapps::AppId& app_id,
    const base::flat_map<Locale, blink::Manifest::TranslationItem>&
        translations,
    WriteCallback callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::move(callback).Run(true);
    return;
  }

  const std::string& locale = g_browser_process->GetApplicationLocale();
  // TODO(crbug.com/40201597): Check other matching locales. Eg if no name
  // defined in en-US, check en.
  auto it = translations.find(locale);
  if (it != translations.end()) {
    translation_cache_[app_id] = it->second;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(WriteTranslationsBlocking, provider_->file_utils(),
                     web_apps_directory_, app_id, translations),
      std::move(callback));
}

void WebAppTranslationManager::DeleteTranslations(const webapps::AppId& app_id,
                                                  WriteCallback callback) {
  if (!base::FeatureList::IsEnabled(
          blink::features::kWebAppEnableTranslations)) {
    std::move(callback).Run(true);
    return;
  }

  translation_cache_.erase(app_id);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(DeleteTranslationsBlocking, provider_->file_utils(),
                     web_apps_directory_, app_id),
      std::move(callback));
}

void WebAppTranslationManager::ReadTranslations(ReadCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(ReadProtoBlocking, provider_->file_utils(),
                     web_apps_directory_),
      base::BindOnce(&WebAppTranslationManager::OnTranslationsRead,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebAppTranslationManager::OnTranslationsRead(
    ReadCallback callback,
    const AllTranslations& proto) {
  translation_cache_.clear();
  const std::string& locale = g_browser_process->GetApplicationLocale();

  for (const auto& id_to_translations : proto.id_to_translations_map()) {
    const webapps::AppId& app_id = id_to_translations.first;

    for (const auto& locale_to_overrides :
         id_to_translations.second.locale_to_overrides_map()) {
      // TODO(crbug.com/40201597): Check other matching locales. Eg if no name
      // defined in en-US, check en.
      if (locale_to_overrides.first == locale) {
        translation_cache_[app_id] =
            ConvertLocaleOverridesToTranslationItem(locale_to_overrides.second);
      }
    }
  }
  std::move(callback).Run(translation_cache_);
}

std::string WebAppTranslationManager::GetTranslatedName(
    const webapps::AppId& app_id) {
  auto it = translation_cache_.find(app_id);
  if (it != translation_cache_.end() && it->second.name) {
    return it->second.name.value();
  }
  return std::string();
}

std::string WebAppTranslationManager::GetTranslatedDescription(
    const webapps::AppId& app_id) {
  auto it = translation_cache_.find(app_id);
  if (it != translation_cache_.end() && it->second.description) {
    return it->second.description.value();
  }
  return std::string();
}

}  // namespace web_app
