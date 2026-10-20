// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_STATS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_STATS_H_

namespace autofill {

// Table-level statistics for the Autocomplete table.
struct AutocompleteTableStats {
  int total_entries = 0;
  int entries_with_label = 0;
  int entries_reused = 0;  // count > 1

  bool operator==(const AutocompleteTableStats&) const = default;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_STATS_H_
