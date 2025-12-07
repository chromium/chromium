// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ExtensionApprovalsParams, ExtensionPermission, ParentAccessParams, WebApprovalsParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {ParentAccessParams_FlowType} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';

export function buildWebApprovalsParams(): ParentAccessParams {
  const webApprovalsParams: WebApprovalsParams = {
    url: {url: 'https://testing.com'},
    childDisplayName: 'Child name',
    faviconPngBytes: [],
  };
  const parentAccessParams: ParentAccessParams = {
    flowType: ParentAccessParams_FlowType.kWebsiteAccess,
    flowTypeParams: {webApprovalsParams},
    isDisabled: false,
  };
  return parentAccessParams;
}

export function buildExtensionApprovalsParamsWithPermissions(
    isDisabled: boolean = false,
    hasDetails: boolean = false): ParentAccessParams {
  const permission: ExtensionPermission = {
    permission: 'permission',
    details: hasDetails ? 'details' : '',
  };

  const extensionApprovalsParams: ExtensionApprovalsParams = {
    extensionName: 'Extension name',
    iconPngBytes: [],
    childDisplayName: 'Child Name',
    permissions: [permission],
  };

  const parentAccessParams: ParentAccessParams = {
    flowType: ParentAccessParams_FlowType.kExtensionAccess,
    flowTypeParams: {extensionApprovalsParams},
    isDisabled: isDisabled,
  };

  return parentAccessParams;
}

export function buildExtensionApprovalsParamsWithoutPermissions(
    isDisabled: boolean = false): ParentAccessParams {
  const extensionApprovalsParams: ExtensionApprovalsParams = {
    extensionName: 'Extension name',
    iconPngBytes: [],
    childDisplayName: 'Child Name',
    permissions: [],
  };

  const parentAccessParams: ParentAccessParams = {
    flowType: ParentAccessParams_FlowType.kExtensionAccess,
    isDisabled: isDisabled,
    flowTypeParams: {extensionApprovalsParams},
  };

  return parentAccessParams;
}

export function clearDocumentBody() {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
}
