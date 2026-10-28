// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_http_user_agent_settings.h"

#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "chromecast/app/grit/chromecast_settings.h"
#include "chromecast/common/user_agent.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_util.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/i18n/android_locale.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

base::i18n::LanguageTag GetLocale() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(byungchul): Use transient locale set when new app starts.
  return base::i18n::GetAndroidDefaultLocale();
#else
  return base::i18n::GetDefaultIcuLocale();
#endif
}

std::string LocaleToAcceptLanguage(const base::i18n::LanguageTag& locale) {
  return net::HttpUtil::GenerateAcceptLanguageHeader(
#if BUILDFLAG(IS_ANDROID)
      std::string(locale.tag_string())
#else
      // Ignoring |locale| here is a bit weird, but locale is still used to
      // avoid a l10n_util::GetStringUTF8() call when GetAcceptLanguage() is
      // called.
      l10n_util::GetStringUTF8(IDS_CHROMECAST_SETTINGS_ACCEPT_LANGUAGES)
#endif
  );
}

}  // namespace

namespace chromecast {
namespace shell {

std::string CastHttpUserAgentSettings::AcceptLanguage() {
  return LocaleToAcceptLanguage(GetLocale());
}

}  // namespace shell
}  // namespace chromecast
