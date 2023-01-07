// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory} from './optimization_guide_internals.mojom-webui.js';

export class OptimizationGuideInternalsBrowserProxy {
  private callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): OptimizationGuideInternalsBrowserProxy {
    return instance ||
        (instance = new OptimizationGuideInternalsBrowserProxy());
  }

  getCallbackRouter(): PageCallbackRouter {
    return this.callbackRouter;
  }
}

let instance: OptimizationGuideInternalsBrowserProxy|null = null;