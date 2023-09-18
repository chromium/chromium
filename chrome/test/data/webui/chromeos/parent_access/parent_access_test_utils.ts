// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ExtensionApprovalsParams, ExtensionPermission, ParentAccessParams, ParentAccessParams_FlowType, WebApprovalsParams} from 'chrome://parent-access/parent_access_ui.mojom-webui.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';

function strToMojoString16(str: string): String16 {
  return {data: str.split('').map(ch => ch.charCodeAt(0))};
}

export function buildWebApprovalsParams(): ParentAccessParams {
  const webApprovalsParams: WebApprovalsParams = {
    url: {url: 'https://testing.com'},
    childDisplayName: strToMojoString16('Child name'),
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
    permission: strToMojoString16('permission'),
    details: hasDetails ? strToMojoString16('details') : strToMojoString16(''),
  };

  const extensionApprovalsParams: ExtensionApprovalsParams = {
    extensionName: strToMojoString16('Extension name'),
    iconPngBytes: [],
    childDisplayName: strToMojoString16('Child Name'),
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
    extensionName: strToMojoString16('Extension name'),
    iconPngBytes: [],
    childDisplayName: strToMojoString16('Child Name'),
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
