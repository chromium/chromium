// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/prefs_util.h"

#include <string>

#include "base/values.h"
#include "components/prefs/pref_service.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/backend/printing_restrictions.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace {

constexpr char kPrintingPaperSizeDefault[] = "printing.paper_size_default";

}  // namespace

namespace printing {

absl::optional<gfx::Size> ParsePaperSizeDefault(const PrefService& prefs) {
  if (!prefs.HasPrefPath(kPrintingPaperSizeDefault))
    return absl::nullopt;

  const base::Value* paper_size_value = prefs.Get(kPrintingPaperSizeDefault);
  if (!paper_size_value || paper_size_value->DictEmpty())
    return absl::nullopt;

  const base::Value* custom_size =
      paper_size_value->FindKey(kPaperSizeCustomSize);
  if (custom_size) {
    return gfx::Size(*custom_size->FindIntKey(kPaperSizeWidth),
                     *custom_size->FindIntKey(kPaperSizeHeight));
  }

  const std::string* name = paper_size_value->FindStringKey(kPaperSizeName);
  DCHECK(name);
  return ParsePaper(*name).size_um;
}

}  // namespace printing
