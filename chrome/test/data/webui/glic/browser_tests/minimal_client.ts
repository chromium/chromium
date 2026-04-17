// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {GlicBrowserHost, GlicWebClient} from '/glic/glic_api/glic_api.js';

import {createGlicHostRegistryOnLoad} from '../api_boot.js';

class WebClient implements GlicWebClient {
  async initialize(_browser: GlicBrowserHost): Promise<void> {}
}

const client = new WebClient();

createGlicHostRegistryOnLoad().then((registry) => {
  registry.registerWebClient(client);
});
