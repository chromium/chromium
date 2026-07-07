// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import type {MetricsInternalsBrowserProxy, RuntimeMutableFeature} from './browser_proxy.js';
import {MetricsInternalsBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './runtime_mutable_features.html.js';

export class RuntimeMutableFeaturesAppElement extends CustomElement {
  static get is(): string {
    return 'runtime-mutable-features-app';
  }

  static override get template() {
    return getTemplate();
  }

  private proxy_: MetricsInternalsBrowserProxy =
      MetricsInternalsBrowserProxyImpl.getInstance();

  private isPaused_: boolean = false;
  private selectedFile_: File|null = null;
  private intervalId_: number|null = null;

  constructor() {
    super();
    this.init_();
  }

  private async init_() {
    this.shadowRoot!.querySelector('#pause-resume-btn')!.addEventListener(
        'click', () => this.togglePauseResume_());
    this.shadowRoot!.querySelector('#seed-file-picker')!.addEventListener(
        'change', (e: Event) => this.onFileSelected_(e));
    this.shadowRoot!.querySelector('#load-seed-btn')!.addEventListener(
        'click', () => this.loadSeed_());

    await this.updateUi_();

    // Periodically refresh the UI every 3 seconds.
    this.intervalId_ = window.setInterval(() => this.updateUi_(), 3000);
  }

  disconnectedCallback() {
    if (this.intervalId_ !== null) {
      window.clearInterval(this.intervalId_);
      this.intervalId_ = null;
    }
  }

  private async updateUi_() {
    await this.updateIsSeedFetchingPaused_();
    await this.updateFeaturesList_();
  }

  private async updateIsSeedFetchingPaused_() {
    this.isPaused_ = await this.proxy_.isSeedFetchingPaused();
    this.updatePauseResumeUi_(this.isPaused_);
  }

  private async updateFeaturesList_() {
    const features: RuntimeMutableFeature[] =
        await this.proxy_.fetchRuntimeMutableFeatures();

    const tbody = this.shadowRoot!.querySelector('#features-body')!;
    tbody.replaceChildren();

    const template =
        this.getRequiredElement<HTMLTemplateElement>('#feature-row-template');
    for (const feature of features) {
      const row = template.content.cloneNode(true) as HTMLElement;
      row.querySelector('.feature-name')!.textContent = feature.name;
      row.querySelector('.feature-state')!.textContent =
          feature.enabled ? 'Enabled' : 'Disabled';
      row.querySelector('.feature-field-trial')!.textContent =
          feature.fieldTrial;
      row.querySelector('.feature-field-trial-group')!.textContent =
          feature.fieldTrialGroup;
      const overrideIcon = row.querySelector<HTMLElement>('.override-icon')!;
      overrideIcon.style.display =
          feature.runtimeOverride ? 'inline-block' : 'none';
      tbody.appendChild(row);
    }
  }

  private async togglePauseResume_() {
    const nextPaused = !this.isPaused_;
    await this.proxy_.setSeedFetchingPaused(nextPaused);
    this.isPaused_ = nextPaused;
    this.updatePauseResumeUi_(this.isPaused_);
  }

  private updatePauseResumeUi_(paused: boolean) {
    const btn = this.shadowRoot!.querySelector('#pause-resume-btn')!;
    const status = this.shadowRoot!.querySelector('#fetch-status')!;
    if (paused) {
      btn.textContent = 'Resume Seed Fetching';
      status.textContent = 'Status: Paused';
    } else {
      btn.textContent = 'Pause Seed Fetching';
      status.textContent = 'Status: Active';
    }
  }

  private onFileSelected_(e: Event) {
    const input = e.target as HTMLInputElement;
    if (input.files && input.files.length > 0) {
      this.selectedFile_ = input.files[0] ?? null;
      this.shadowRoot!.querySelector('#load-seed-btn')!.removeAttribute(
          'disabled');
    } else {
      this.selectedFile_ = null;
      this.shadowRoot!.querySelector('#load-seed-btn')!.setAttribute(
          'disabled', '');
    }
  }

  private loadSeed_() {
    if (!this.selectedFile_) {
      return;
    }

    const reader = new FileReader();
    reader.onload = async (e) => {
      const buffer = e.target?.result as ArrayBuffer;
      const bytes = new Uint8Array(buffer);

      await this.proxy_.uploadSeed(bytes);
      // Refresh list after loading seed as it might have changed feature
      // states
      await this.updateFeaturesList_();
    };
    reader.readAsArrayBuffer(this.selectedFile_);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'runtime-mutable-features-app': RuntimeMutableFeaturesAppElement;
  }
}

customElements.define(
    RuntimeMutableFeaturesAppElement.is, RuntimeMutableFeaturesAppElement);
