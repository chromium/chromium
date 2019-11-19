// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/renovations/page_renovation_loader.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace offline_pages {

namespace {

// Helper function used in WikipediaPageRenovation::ShouldRun.
bool EndsWith(const std::string& host, const std::string& suffix) {
  if (suffix.size() > host.size())
    return false;

  return std::equal(suffix.rbegin(), suffix.rend(), host.rbegin());
}

// Concrete PageRenovation instances
class WikipediaPageRenovation : public PageRenovation {
 public:
  bool ShouldRun(const GURL& url) const override {
    return EndsWith(url.host(), "m.wikipedia.org");
  }

  std::string GetID() const override { return "wikipedia"; }
};

// Construct list of implemented renovations
std::vector<std::unique_ptr<PageRenovation>> MakeRenovationList() {
  std::vector<std::unique_ptr<PageRenovation>> list;
  list.emplace_back(new WikipediaPageRenovation);
  return list;
}

}  // namespace

PageRenovationLoader::PageRenovationLoader()
    : renovations_(MakeRenovationList()), is_loaded_(false) {}

PageRenovationLoader::~PageRenovationLoader() {}

bool PageRenovationLoader::GetRenovationScript(
    const std::vector<std::string>& renovation_ids,
    base::string16* script) {
  if (!LoadSource()) {
    return false;
  }

  // The loaded script contains a function called run_renovations
  // which takes an array of renovation names and runs all the
  // associated renovation code. Create call to run_renovations
  // function in JavaScript file.
  std::string jsCall = "\nrun_renovations([";
  for (const std::string& renovation_id : renovation_ids) {
    jsCall += "\"";
    jsCall += renovation_id;
    jsCall += "\",";
  }
  jsCall += "]);";

  // Append run_renovations call to combined_source_, which contains
  // the definition of run_renovations.
  *script = combined_source_;
  *script += base::UTF8ToUTF16(jsCall);

  return true;
}

bool PageRenovationLoader::LoadSource() {
  // We only need to load the script from storage once.
  if (is_loaded_) {
    return true;
  }

  // Our script file is stored in the resource bundle. Get this script.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  combined_source_ = base::UTF8ToUTF16(
      rb.LoadDataResourceString(IDR_OFFLINE_PAGES_RENOVATIONS_JS));

  // If built correctly, IDR_OFFLINE_PAGES_RENOVATIONS_JS should
  // always exist in the resource pack and loading should never fail.
  DCHECK_GT(combined_source_.size(), 0U);

  is_loaded_ = true;
  return true;
}

void PageRenovationLoader::SetSourceForTest(base::string16 combined_source) {
  combined_source_ = std::move(combined_source);
  is_loaded_ = true;
}

void PageRenovationLoader::SetRenovationsForTest(
    std::vector<std::unique_ptr<PageRenovation>> renovations) {
  renovations_ = std::move(renovations);
}

}  // namespace offline_pages
