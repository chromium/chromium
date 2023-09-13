// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/net_resource_provider.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/values.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "net/grit/net_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/jstemplate_builder.h"

namespace {

// The net module doesn't have access to this HTML or the strings that need to
// be localized.  The Chrome locale will never change while we're running, so
// it's safe to have a static string that we always return a pointer into.
struct LazyDirectoryListerCacher {
  LazyDirectoryListerCacher() {
    base::Value::Dict value;
    value.Set("header", l10n_util::GetStringUTF8(IDS_DIRECTORY_LISTING_HEADER));
    value.Set("parentDirText",
              l10n_util::GetStringUTF8(IDS_DIRECTORY_LISTING_PARENT));
    value.Set("headerName",
              l10n_util::GetStringUTF8(IDS_DIRECTORY_LISTING_NAME));
    value.Set("headerSize",
              l10n_util::GetStringUTF8(IDS_DIRECTORY_LISTING_SIZE));
    value.Set("headerDateModified",
              l10n_util::GetStringUTF8(IDS_DIRECTORY_LISTING_DATE_MODIFIED));
    value.Set("language",
              l10n_util::GetLanguage(base::i18n::GetConfiguredLocale()));
    value.Set("textdirection", base::i18n::IsRTL() ? "rtl" : "ltr");
    std::string str = webui::GetI18nTemplateHtml(
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_DIR_HEADER_HTML),
        value);

    html_data = base::MakeRefCounted<base::RefCountedString>(std::move(str));
  }

  scoped_refptr<base::RefCountedMemory> html_data;
};

}  // namespace

scoped_refptr<base::RefCountedMemory> ChromeNetResourceProvider(int key) {
  static base::NoDestructor<LazyDirectoryListerCacher> lazy_dir_lister;

  if (IDR_DIR_HEADER_HTML == key)
    return lazy_dir_lister->html_data;

  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(key);
}
