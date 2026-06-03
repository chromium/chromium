// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IntroPageCallbackRouter} from 'chrome://intro/intro.mojom-webui.js';
import type {IntroBrowserProxy} from 'chrome://intro/intro_browser_proxy.js';

export class TestIntroMojoBrowserProxy implements IntroBrowserProxy {
  callbackRouter: IntroPageCallbackRouter = new IntroPageCallbackRouter();
}
