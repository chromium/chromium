// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/net_resource_provider.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "net/grit/net_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

namespace {

// The net module doesn't have access to this HTML or the strings that need to
// be localized.  The Chrome locale will never change while we're running, so
// it's safe to have a static string that we always return a pointer into.
// This allows us to have the ResourceProvider return a pointer into the actual
// resource (via a StringPiece), instead of always copying resources.
struct LazyDirectoryListerCacher {
  LazyDirectoryListerCacher() {
    base::DictionaryValue value;
    value.SetString("header",
                    l10n_util::GetStringUTF16(IDS_DIRECTORY_LISTING_HEADER));
    value.SetString("parentDirText",
                    l10n_util::GetStringUTF16(IDS_DIRECTORY_LISTING_PARENT));
    value.SetString("headerName",
                    l10n_util::GetStringUTF16(IDS_DIRECTORY_LISTING_NAME));
    value.SetString("headerSize",
                    l10n_util::GetStringUTF16(IDS_DIRECTORY_LISTING_SIZE));
    value.SetString("headerDateModified",
        l10n_util::GetStringUTF16(IDS_DIRECTORY_LISTING_DATE_MODIFIED));
    value.SetString("language",
                    l10n_util::GetLanguage(base::i18n::GetConfiguredLocale()));
    value.SetString("listingParsingErrorBoxText",
        l10n_util::GetStringFUTF16(IDS_DIRECTORY_LISTING_PARSING_ERROR_BOX_TEXT,
            l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)));
    value.SetString("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");
    html_data = webui::GetI18nTemplateHtml(
        ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
            IDR_DIR_HEADER_HTML),
        &value);
  }

  std::string html_data;
};

}  // namespace

namespace chrome_common_net {

base::StringPiece NetResourceProvider(int key) {
  static base::NoDestructor<LazyDirectoryListerCacher> lazy_dir_lister;

  if (IDR_DIR_HEADER_HTML == key)
    return base::StringPiece(lazy_dir_lister->html_data);

  return ui::ResourceBundle::GetSharedInstance().GetRawDataResource(key);
}

}  // namespace chrome_common_net
