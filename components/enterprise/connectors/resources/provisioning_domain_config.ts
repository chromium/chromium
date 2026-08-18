// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {BrowserProxy} from './browser_proxy.js';
import type {PageHandlerInterface, ProvisioningDomainConfig, ProvisioningDomainState} from './connectors_internals.mojom-webui.js';
import {getTemplate} from './provisioning_domain_config.html.js';

export class ProvisioningDomainConfigElement extends CustomElement {
  static get is() {
    return 'provisioning-domain-config';
  }

  static override get template() {
    return getTemplate();
  }

  constructor() {
    super();
    this.fetchProvisioningDomainState();
  }

  private get pageHandler(): PageHandlerInterface {
    return BrowserProxy.getInstance().handler;
  }

  private get noConfigsMessage(): HTMLElement | null {
    return this.$('#no-configs-message');
  }

  private get pvdConfigsList(): HTMLElement | null {
    return this.$('#pvd-configs-list');
  }

  private fetchProvisioningDomainState() {
    this.pageHandler.getProvisioningDomainState().then(
        (response: {state: ProvisioningDomainState}) => this.updateState(response.state),
        err => console.error(
            `Failed to fetch Provisioning Domain state: ${JSON.stringify(err)}`));
  }

  private updateState(state: ProvisioningDomainState) {
    const listElement = this.pvdConfigsList;
    const messageElement = this.noConfigsMessage;
    if (!listElement || !messageElement) {
      return;
    }

    const configs = state.pvdConfigs;
    if (!configs || configs.length === 0) {
      messageElement.classList.remove('hidden');
      listElement.replaceChildren();
      return;
    }

    messageElement.classList.add('hidden');
    const configElements = configs.map(config => this.createProvisioningDomainConfigElement(config));
    listElement.replaceChildren(...configElements);
  }

  private createProvisioningDomainConfigElement(config: ProvisioningDomainConfig): HTMLElement {
    const section = document.createElement('div');
    section.classList.add('pvd-section');

    const title = document.createElement('h3');
    title.textContent = `Provisioning Domain ID: ${config.pvdId}`;
    section.appendChild(title);

    section.appendChild(
        this.createLabelledValueElement(
            'Expiration Time', config.expirationTime ? config.expirationTime.toString() : 'None'));

    if (config.policyJson) {
      section.appendChild(
          this.createCodeBlockElement('Policies Set', config.policyJson));
    }

    if (config.routesJson) {
      section.appendChild(
          this.createCodeBlockElement('Fetched Configuration', config.routesJson));
    }

    return section;
  }

  private createLabelledValueElement(label: string, text: string): HTMLElement {
    const valueSpan = document.createElement('span');
    valueSpan.classList.add('bold');
    valueSpan.textContent = text;

    const containerElement = document.createElement('div');
    containerElement.append(`${label}: `, valueSpan);
    return containerElement;
  }

  private createCodeBlockElement(label: string, jsonText: string): HTMLElement {
    const container = document.createElement('div');
    const labelDiv = document.createElement('div');
    labelDiv.textContent = `${label}:`;
    labelDiv.classList.add('bold');

    const pre = document.createElement('pre');
    pre.textContent = jsonText;

    container.append(labelDiv, pre);
    return container;
  }
}

customElements.define(ProvisioningDomainConfigElement.is, ProvisioningDomainConfigElement);
