// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * The maximum length allowed for form data.
 *
 * This variable is from AutofillTable::kMaxDataLength in
 * chromium/src/components/autofill/core/browser/webdata/autofill_table.h
 */
const MAX_DATA_LENGTH = 1024;

/**
 * The maximum string length supported by Autofill.
 *
 * This variable is from kMaxStringLength in
 * chromium/src/components/autofill/core/common/autofill_constant.h
 */
const MAX_STRING_LENGTH = 1024;

/**
 * The maximum number of form fields we are willing to parse, due to
 * computational costs. Several examples of forms with lots of fields that are
 * not relevant to Autofill: (1) the Netflix queue; (2) the Amazon wishlist;
 * (3) router configuration pages; and (4) other configuration pages, e.g. for
 * Google code project settings.
 *
 * This variable is `kMaxExtractableFields` from
 * chromium/src/components/autofill/core/common/autofill_constants.h
 */
const MAX_EXTRACTABLE_FIELDS = 200;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting value from WebFormControlElement.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 */
const EXTRACT_MASK_VALUE = 1 << 0;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting option text from WebFormSelectElement. Only valid when
 * EXTRACT_MASK_VALUE is set. This is used for form submission where human
 * readable value is captured.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 */
const EXTRACT_MASK_OPTION_TEXT = 1 << 1;

/**
 * A bit field mask to extract data from WebFormControlElement for
 * extracting options from WebFormControlElement.
 *
 * This variable is from enum ExtractMask in
 * chromium/src/components/autofill/content/renderer/form_autofill_util.h
 */
const EXTRACT_MASK_OPTIONS = 1 << 2;

/**
 * A value for the "presentation" role.
 *
 * This variable is from enum RoleAttribute in
 * chromium/src/components/autofill/core/common/form_field_data.h
 */
const ROLE_ATTRIBUTE_PRESENTATION = 0;

/**
 * The value for a unique form or field ID not set or missing.
 */
const RENDERER_ID_NOT_SET = '0';

/**
 * The JS Symbol object used to set stable unique form and field IDs.
 */
const ID_SYMBOL = window.Symbol.for('__gChrome~uniqueID');

export {
  MAX_DATA_LENGTH,
  MAX_STRING_LENGTH,
  MAX_EXTRACTABLE_FIELDS,
  EXTRACT_MASK_VALUE,
  EXTRACT_MASK_OPTION_TEXT,
  EXTRACT_MASK_OPTIONS,
  ROLE_ATTRIBUTE_PRESENTATION,
  RENDERER_ID_NOT_SET,
};

gCrWeb.fill.ID_SYMBOL = ID_SYMBOL;
