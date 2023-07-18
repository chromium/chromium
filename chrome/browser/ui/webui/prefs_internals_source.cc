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
#include "components/local_state/local_state_utils.h"
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
  std::move(callback).Run(base::MakeRefCounted<base::RefCountedString>(
      local_state_utils::GetPrefsAsJson(profile_->GetPrefs())
          .value_or(std::string())));
}
