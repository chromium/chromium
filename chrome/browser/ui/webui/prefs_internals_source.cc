// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/prefs_internals_source.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

PrefsInternalsSource::PrefsInternalsSource(Profile* profile)
    : profile_(profile) {}

PrefsInternalsSource::~PrefsInternalsSource() = default;

std::string PrefsInternalsSource::GetSource() {
  return chrome::kChromeUIPrefsInternalsHost;
}

std::string PrefsInternalsSource::GetMimeType(const GURL& url) {
  return "text/plain";
}

void PrefsInternalsSource::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    content::URLDataSource::GotDataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::string json;
  base::Value prefs =
      profile_->GetPrefs()->GetPreferenceValues(PrefService::INCLUDE_DEFAULTS);
  CHECK(base::JSONWriter::WriteWithOptions(
      prefs, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json));

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(json)));
}
