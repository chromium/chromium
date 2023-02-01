// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ParentAccessParams, ParentAccessParams_FlowType, WebApprovalsParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';

function strToMojoString16(str) {
  return {data: str.split('').map(ch => ch.charCodeAt(0))};
}

/**
 * @returns {ParentAccessParams}
 */
export function buildWebApprovalsParams() {
  const parentAccessParams = new ParentAccessParams();
  parentAccessParams.flowType = ParentAccessParams_FlowType.kWebsiteAccess;
  const webApprovalsParams = new WebApprovalsParams();
  webApprovalsParams.url = {url: 'https://testing.com'};
  webApprovalsParams.childDisplayName = strToMojoString16('Child Name');
  webApprovalsParams.faviconPngBytes = [];
  parentAccessParams.flowTypeParams = {webApprovalsParams};
  return parentAccessParams;
}

/**
 * @param {boolean} isDisabled If the disabled UI should be shown.
 * @returns {ParentAccessParams}
 */
export function buildExtensionApprovalsParams(isDisabled) {
  const parentAccessParams = new ParentAccessParams();
  parentAccessParams.flowType = ParentAccessParams_FlowType.kExtensionAccess;
  parentAccessParams.isDisabled = isDisabled;
  return parentAccessParams;
}
