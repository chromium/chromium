// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Add type extensions needed for other scripts defining the fill namespace.
declare global {
  // Defines an additional property, `angular`, on the Window object.
  // The code below assumes that this property exists within the object.
  interface Window {
    angular: any;
  }

  // Extends the Document object to add the ability to access its
  // properties via the [] notation and defines a property that is
  // assumed to exist within the object.
  interface Document {
    [key: symbol]: number;
    __gCrElementMap: Map<any, any>;
    __gCrWasEditedByUserMap: WeakMap<any, any>;
    __gCrWebURLNormalizer: HTMLAnchorElement;

    /**
     * Registry that tracks the forms that were submitted during the frame's
     * lifetime. Elements that are garbage collected will be removed from the
     * registry so this can't memory leak. In the worst case the registry will
     * get as big as the number of submitted forms that aren't yet deleted and
     * we don't expect a lot of those.
     */
    __gCrFormSubmissionRegistry: WeakSet<any>;
  }
}

export declare type FormControlElement =
    HTMLInputElement | HTMLTextAreaElement | HTMLSelectElement;

/**
 * The maximum length allowed for form data.
 *
 * This variable is from AutofillTable::kMaxDataLength in
 * chromium/src/components/autofill/core/browser/webdata/autofill_table.h
 */
export const MAX_DATA_LENGTH = 1024;

/**
 * The maximum string length supported by Autofill.
 *
 * This variable is from kMaxStringLength in
 * chromium/src/components/autofill/core/common/autofill_constant.h
 */
export const MAX_STRING_LENGTH = 1024;

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
export const MAX_EXTRACTABLE_FIELDS = 200;

// The maximum number of frames we are willing to extract, due to computational
// costs.
export const MAX_EXTRACTABLE_FRAMES = 20;

/**
 * A value for the "presentation" role.
 *
 * This variable is from enum RoleAttribute in
 * chromium/src/components/autofill/core/common/form_field_data.h
 */
export const ROLE_ATTRIBUTE_PRESENTATION = 0;

/**
 * The value for a unique form or field ID not set or missing.
 */
export const RENDERER_ID_NOT_SET = '0';

/**
 Name of the html attribute used for storing stable unique form and field IDs.
 */
export const UNIQUE_ID_ATTRIBUTE = '__gCrUniqueID';

/**
 * The JS Symbol object used to set stable unique form and field IDs.
 */
export const ID_SYMBOL = window.Symbol.for(UNIQUE_ID_ATTRIBUTE);

/**
 Name of the html attribute used for storing the remote frame token assigned to
 a child frame. Stored as an attribute of the iframe html element hosting the
 child frame.
 */
export const CHILD_FRAME_REMOTE_TOKEN_ATTRIBUTE = '__gCrChildFrameRemoteToken';
