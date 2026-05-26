// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import path from 'node:path';
import fs from 'node:fs';

/**
 * A Rollup plugin that resolves imports for the variations parser.
 * It handles mapping the @bufbuild/protobuf wire reader to its local
 * ESM distribution and links the parser to its generated proto bindings.
 *
 * @param {string} nodeModulesPath Absolute path to the @bufbuild/protobuf
 *     ESM distribution.
 * @param {string} protoJsPath Absolute path to the transpiled
 *     client_variations.js proto bindings.
 */
export function variationsResolver(nodeModulesPath, protoJsPath) {
  return {
    name: 'custom-resolver',
    resolveId(source, importee) {
      if (source.includes('@bufbuild/protobuf/')) {
        const parts = source.split('@bufbuild/protobuf/');
        const subpath = parts.at(-1);

        // Try direct path
        const p = path.resolve(nodeModulesPath, subpath);
        if (fs.existsSync(p)) {
          return p;
        }

        // Try as directory with index.js
        const pIdx = path.resolve(nodeModulesPath, subpath, 'index.js');
        if (fs.existsSync(pIdx)) {
          return pIdx;
        }

        // Try with .js extension
        const pJs = path.resolve(nodeModulesPath, subpath + '.js');
        if (fs.existsSync(pJs)) {
          return pJs;
        }
      }

      return source === 'client_variations_proto' ? protoJsPath : null;
    }
  };
}
