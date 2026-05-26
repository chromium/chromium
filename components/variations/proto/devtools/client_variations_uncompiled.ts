// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClientVariations} from 'client_variations_proto';

const VARIATION_IDS_COMMENT =
    'Active Google-visible variation IDs on this client. These are ' +
    'reported for analysis, but do not directly affect any server-side ' +
    'behavior.';
const TRIGGER_VARIATION_IDS_COMMENT =
    'Active Google-visible variation IDs on this client that trigger ' +
    'server-side behavior. These are reported for analysis *and* directly ' +
    'affect server-side behavior.';

/**
 * Parses a base64-serialized ClientVariations proto into a human-readable format.
 * @param data The base64-serialized ClientVariations proto contents, e.g.
 *     taken from the X-Client-Data header.
 */
export function parseClientVariations(data: string): {
  variationIds: number[],
  triggerVariationIds: number[],
} {
  if (data === '') {
    return {
      variationIds: [],
      triggerVariationIds: [],
    };
  }

  let decoded = '';
  try {
    decoded = atob(data);
  } catch (e) {
    // If base64 decoding fails, we cannot proceed.
    return {
      variationIds: [],
      triggerVariationIds: [],
    };
  }

  const bytes = new Uint8Array(decoded.length);
  for (let i = 0; i < decoded.length; i++) {
    bytes[i] = decoded.charCodeAt(i);
  }

  let parsed: ClientVariations;
  try {
    parsed = ClientVariations.decode(bytes);
  } catch (e) {
    // Deserialization is never expected to fail in Chromium,
    // but might fail in downstream repositories such as Edgium or
    // if any website uses the same header name 'x-client-data'
    return {
      variationIds: [],
      triggerVariationIds: [],
    };
  }
  return {
    variationIds: parsed.variationId ?? [],
    triggerVariationIds: parsed.triggerVariationId ?? [],
  };
}

/**
 * Formats a parsed ClientVariations proto into a human-readable representation
 * of the proto, which echoes the proto definition in
 * https://source.chromium.org/chromium/chromium/src/+/main:components/variations/proto/client_variations.proto;l=14-22
 */
export function formatClientVariations(
    data: {variationIds: number[], triggerVariationIds: number[]},
    variationIdsComment: string = VARIATION_IDS_COMMENT,
    triggerVariationIdsComment: string =
        TRIGGER_VARIATION_IDS_COMMENT): string {
  const variationIds = data.variationIds;
  const triggerVariationIds = data.triggerVariationIds;
  const buffer = ['message ClientVariations {'];
  if (variationIds && variationIds.length) {
    const ids = variationIds.join(', ');
    buffer.push(
        `  // ${variationIdsComment}`,
        `  repeated int32 variation_id = [${ids}];`);
  }
  if (triggerVariationIds && triggerVariationIds.length) {
    const ids = triggerVariationIds.join(', ');
    buffer.push(
        `  // ${triggerVariationIdsComment}`,
        `  repeated int32 trigger_variation_id = [${ids}];`);
  }
  buffer.push('}');
  return buffer.join('\n');
}
