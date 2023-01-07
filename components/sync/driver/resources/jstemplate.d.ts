// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare class JsEvalContext {
  constructor(data: any);
}
declare function jstProcess(
    context: JsEvalContext, template: HTMLElement): void;
