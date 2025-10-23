// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NodeIdDataView, NodeIdTypeMapper} from './tab_strip_api_types.mojom-converters.js';
import type {NodeId_Type} from './tab_strip_api_types.mojom-webui.js';

const SEPARATOR = ':';

export class NodeIdConverter implements NodeIdTypeMapper<string> {
  private validateAndSplit(token: string): [string, string] {
    const parts = token.split(SEPARATOR);
    if (parts.length !== 2 || !parts[0] || !parts[1]) {
      throw new Error(`NodeId token is malformed. "${token}".`);
    }
    return parts as [string, string];
  }

  // Encoding
  id(token: string): string {
    const split = this.validateAndSplit(token);
    return split[0];
  }

  type(token: string): NodeId_Type {
    const split = this.validateAndSplit(token);
    const typeNum = parseInt(split[1]);
    return typeNum as NodeId_Type;
  }

  // Decoding
  convert(view: NodeIdDataView): string {
    return `${view.id}${SEPARATOR}${view.type}`;
  }
}
