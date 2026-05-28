// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ClientVariations = goog.require('proto.variations.ClientVariations');

const VARIATION_IDS_COMMENT =
      'Active Google-visible variation IDs on this client. These are ' +
      'reported for analysis, but do not directly affect any server-side ' +
      'behavior.';
const TRIGGER_VARIATION_IDS_COMMENT =
      'Active Google-visible variation IDs on this client that trigger ' +
      'server-side behavior. These are reported for analysis *and* directly ' +
      'affect server-side behavior.';

/**
 * Parses a serialized ClientVariations proto into a human-readable format.
 * @param {string} data The serialized ClientVariations proto contents, e.g.
 *     taken from the X-Client-Data header.
 * @return {{
 *   variationIds: !Array<number>,
 *   triggerVariationIds: !Array<number>,
 * }}
 */
export function parse(data) {
  let decoded = '';
  try {
    decoded = atob(data);
  } catch (e) {
    // Nothing to do here -- it's fine to leave `decoded` empty if base64
    // decoding fails.
  }

  const bytes = [];
  for (let i = 0; i < decoded.length; i++) {
    bytes.push(decoded.charCodeAt(i));
  }

  let parsed = null;
  try {
    parsed = ClientVariations.deserializeBinary(bytes);
  } catch (e) {
    // Deserialization is never expected to fail in Chromium,
    // but might fail in downstream repositories such as Edgium or
    // if any website uses the same header name 'x-client-data'
    parsed = ClientVariations.deserializeBinary([]);
  }
  return {
    'variationIds': parsed.getVariationIdList(),
    'triggerVariationIds': parsed.getTriggerVariationIdList(),
  };
}

/**
 * Formats a parsed ClientVariations proto into a human-readable representation
 * of the proto, which echoes the proto definition in
 * https://source.chromium.org/chromium/chromium/src/+/main:components/variations/proto/client_variations.proto;l=14-22
 *
 * @param {{
 *   variationIds: !Array<number>,
 *   triggerVariationIds: !Array<number>,
 * }} data
 * @param {string=} variationComment
 * @param {string=} triggerVariationComment
 * @return {string}
 */
export function format(
    data, variationIdsComment = VARIATION_IDS_COMMENT,
    triggerVariationIdsComment = TRIGGER_VARIATION_IDS_COMMENT) {
  const variationIds = data['variationIds'];
  const triggerVariationIds = data['triggerVariationIds'];
  const buffer = ['message ClientVariations {'];
  if (variationIds.length) {
    const ids = variationIds.join(', ');
    buffer.push(
        `  // ${variationIdsComment}`,
        `  repeated int32 variation_id = [${ids}];`);
  }
  if (triggerVariationIds.length) {
    const ids = triggerVariationIds.join(', ');
    buffer.push(
        `  // ${triggerVariationIdsComment}`,
        `  repeated int32 trigger_variation_id = [${ids}];`);
  }
  buffer.push('}');
  return buffer.join('\n');
}

goog.exportSymbol('parseClientVariations', parse);
goog.exportSymbol('formatClientVariations', format);
