// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CLIPBOARD_CONTEXT_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CLIPBOARD_CONTEXT_H_

#include <optional>

#include "components/enterprise/common/proto/connectors.pb.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "url/gurl.h"

namespace data_controls {

// Platform-agnostic representation of clipboard data and metadata for handling
// by the "DataControlsRules" policy.
class ClipboardContext {
 public:
  // Returns a default-constructed GURL if the source/destination isn't a
  // browser tab.
  virtual GURL source_url() const = 0;
  virtual GURL destination_url() const = 0;

  // Returns the context's data source as a `CopiedTextSource` representation
  // for Data Controls. This shouldn't be used for
  // "OnBulkDataEntryEnterpriseConnector" since the internal logic of the
  // function will check the scope at which the policy is set.
  virtual enterprise_connectors::ContentMetaData::CopiedTextSource
  data_controls_copied_text_source() const = 0;

  // The `ui::ClipboardFormatType` of the data represented by this context.
  virtual ui::ClipboardFormatType format_type() const = 0;

  // The total size of the clipboard data in bytes. This is null when files are
  // copied.
  virtual std::optional<size_t> size() const = 0;

  // The active content area user for the source/destination of the clipboard
  // interaction. This returns an empty string for non-Workspace sites or if the
  // user(s) can't be identified.
  virtual std::string source_active_user() const = 0;
  virtual std::string destination_active_user() const = 0;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_CLIPBOARD_CONTEXT_H_
