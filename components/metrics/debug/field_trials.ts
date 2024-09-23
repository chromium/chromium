// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';

import type {FieldTrialState, Group, HashNamed, HashNameMap, MetricsInternalsBrowserProxy, Trial} from './browser_proxy.js';
import {MetricsInternalsBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './field_trials.html.js';

// Stores and persists study and group names along with their hash.
class NameUnhasher {
  private hashNames: Map<string, string> =
      new Map(JSON.parse(localStorage.getItem('names') || '[]'));
  // We remember up to this.maxStoredNames names in localStorage.
  private readonly maxStoredNames = 500;

  add(names: HashNameMap): boolean {
    let changed = false;
    for (const [hash, name] of Object.entries(names)) {
      if (name && !this.hashNames.has(hash)) {
        changed = true;
        this.hashNames.set(hash, name);
      }
    }
    if (changed) {
      // Note: `Map` retains item order, so this keeps the most recent
      // `maxStoredNames` entries.
      localStorage.setItem(
          'names',
          JSON.stringify(Array.from(this.hashNames.entries())
                             .slice(-this.maxStoredNames)));
    }
    return changed;
  }

  displayName(named: HashNamed): string {
    const name = named.name || this.hashNames.get(named.hash);
    return name ? `${name} (#${named.hash})` : `#${named.hash}`;
  }
}

class SearchFilter {
  private searchParts: Array<[string, string]> = [];

  constructor(private unhasher: NameUnhasher, private searchText: string) {
    this.searchText = searchText.toLowerCase();
    // Allow any of these separators. This means we may need to consider more
    // than one interpretation. For example: "One.Two-Three" could be a single
    // name, or one of the trial/group combinations "One/Two-Three",
    // "One.Two/Three".
    for (const separator of '/.:-') {
      const parts = this.searchText.split(separator);
      if (parts.length === 2) {
        this.searchParts.push(parts as [string, string]);
      }
    }
  }

  match(named: HashNamed, checkParts: boolean): MatchResult {
    if (this.searchText === '') {
      return MatchResult.NONE;
    }
    let match = this.matchNameOrHash(this.searchText, named);
    if (!match && checkParts) {
      for (const parts of this.searchParts) {
        match = this.matchNameOrHash(parts['groups' in named ? 0 : 1]!, named);
        if (match) {
          break;
        }
      }
    }
    return match ? MatchResult.MATCH : MatchResult.MISMATCH;
  }

  private matchNameOrHash(search: string, subject: HashNamed): boolean {
    return this.unhasher.displayName(subject).toLowerCase().includes(search);
  }
}

enum MatchResult {
  // There is no search query.
  NONE = '',
  // Matched the search.
  MATCH = 'match',
  // Did not match the search.
  MISMATCH = 'no-match',
}

export class TrialRow {
  root: HTMLDivElement;
  overridden = false;
  experimentRows: ExperimentRow[] = [];

  constructor(private app: FieldTrialsAppElement, public trial: Trial) {
    this.root = document.createElement('div');
    this.root.classList.add('trial-row');
    this.root.innerHTML = getTrustedHTML`
      <div class="trial-header">
        <button class="expand-button"></button>
        <span class="trial-name"></span>
      </div>
      <div class="trial-groups"></div>`;

    for (const group of trial.groups) {
      this.overridden = this.overridden || group.forceEnabled;
      const experimentRow = new ExperimentRow(this.app, trial, group);
      this.experimentRows.push(experimentRow);
    }

    this.root.querySelector('.trial-groups')!.replaceChildren(
        ...this.experimentRows.map(r => r.root));
    this.root.querySelector('.expand-button')!.addEventListener('click', () => {
      const dataset = this.root.dataset;
      dataset['expanded'] = String(dataset['expanded'] !== 'true');
    });
  }

  update() {
    this.root.querySelector('.trial-name')!.replaceChildren(
        this.app.unhasher.displayName(this.trial));
    for (const row of this.experimentRows) {
      row.update();
    }
  }

  findExperimentRow(groupHash: string): ExperimentRow|undefined {
    return this.experimentRows.find(row => row.group.hash === groupHash);
  }

  setMatchResult(result: MatchResult) {
    this.root.dataset['searchResult'] = result;
  }

  filter(searchFilter: SearchFilter): [boolean, number] {
    let matches = 0;
    let trialResult: MatchResult = searchFilter.match(this.trial, true);
    for (const row of this.experimentRows) {
      const result =
          searchFilter.match(row.group, trialResult === MatchResult.MATCH);
      row.setMatchResult(result);
      if (result === MatchResult.MATCH) {
        trialResult = MatchResult.MATCH;
        matches++;
      }
    }
    this.root.dataset['searchResult'] = trialResult;
    return [trialResult === MatchResult.MATCH, matches];
  }

  displayName(): string {
    return this.app.unhasher.displayName(this.trial);
  }

  sortKey(): string {
    const name = this.displayName();
    // Order: Overridden trials, trials with names, trials with hash only.
    return `${Number(!this.overridden)}${Number(name.startsWith('#'))}${name}`;
  }
}

class ExperimentRow {
  root: HTMLDivElement;

  constructor(
      private app: FieldTrialsAppElement, public trial: Trial,
      public group: Group) {
    this.root = document.createElement('div');
    this.root.classList.add('experiment-row');
    this.root.innerHTML = getTrustedHTML`
      <div class="experiment-name"></div>
      <div class="override">
        <label for="override">
          Override <input type="checkbox" name="override">
        </label>
      </div>`;
    if (group.enabled) {
      this.root.dataset['enrolled'] = '1';
    }
    if (group.forceEnabled) {
      this.setForceEnabled(true);
    }
    this.update();
    this.root.querySelector('input')!.addEventListener(
        'click', () => this.app.toggleForceEnable(trial, group));
  }

  update() {
    this.root.querySelector('.experiment-name')!.replaceChildren(
        this.app.unhasher.displayName(this.group));
  }

  setForceEnabled(forceEnabled: boolean) {
    this.group.forceEnabled = forceEnabled;
    const checkbox = this.root.querySelector('input')!;
    checkbox.checked = forceEnabled;
    if (forceEnabled) {
      checkbox.dataset['overridden'] = '1';
    } else {
      delete checkbox.dataset['overridden'];
    }
  }

  setMatchResult(result: MatchResult) {
    this.root.dataset['searchResult'] = result;
  }
}

interface ElementIdMap {
  'restart-button': HTMLElement;
  'needs-restart': HTMLElement;
  'filter': HTMLInputElement;
  'filter-status': HTMLElement;
  'field-trial-list': HTMLElement;
}

export class FieldTrialsAppElement extends CustomElement {
  static get is(): string {
    return 'field-trials-app';
  }

  static override get template() {
    return getTemplate();
  }

  private proxy_: MetricsInternalsBrowserProxy =
      MetricsInternalsBrowserProxyImpl.getInstance();

  // Whether changes require dom updates. Visible for testing.
  dirty = true;
  // The list of available trials.
  private trials: TrialRow[] = [];
  unhasher = new NameUnhasher();

  onUpdateForTesting = () => {};

  private el<K extends keyof ElementIdMap>(id: K): ElementIdMap[K] {
    const result = this.shadowRoot!.getElementById(id) as any;
    assert(result);
    return result;
  }

  constructor() {
    super();
    // Initialize only when this element is first visible.
    new Promise<void>(resolve => {
      const observer = new IntersectionObserver((entries) => {
        if (entries.filter(entry => entry.intersectionRatio > 0).length > 0) {
          resolve();
        }
      });
      observer.observe(this);
    }).then(() => {
      this.init_();
    });
  }

  private init_() {
    this.proxy_.fetchTrialState().then(state => this.populateState_(state));

    // We're using a form to get autocomplete functionality, but don't need
    // submit behavior.
    this.getRequiredElement('form').addEventListener(
        'submit', (e) => e.preventDefault());

    this.filterInputElement.value = localStorage.getItem('filter') || '';
    this.filterInputElement.addEventListener(
        'input', () => this.filterUpdated_());
    this.el('restart-button')
        .addEventListener('click', () => this.proxy_.restart());
    this.filterUpdated_();
  }

  forceUpdateForTesting() {
    this.update_();
  }

  private setRestartRequired_(): void {
    this.dataset['needsRestart'] = 'true';
  }

  private filterUpdated_(): void {
    this.el('filter-status').replaceChildren();
    localStorage.setItem('filter', this.filterInputElement.value);

    this.proxy_.lookupTrialOrGroupName(this.filterInputElement.value)
        .then(names => {
          if (this.unhasher.add(names)) {
            this.setDirty_();
          }
        });
    this.setDirty_();
  }

  private setDirty_() {
    if (this.dirty) {
      return;
    }
    this.dirty = true;
    window.setTimeout(() => this.update_(), 500);
  }

  private update_() {
    if (!this.dirty) {
      return;
    }
    this.dirty = false;
    for (const trial of this.trials) {
      trial.update();
    }
    this.filterToInput_();
    this.onUpdateForTesting();
  }

  get filterInputElement(): HTMLInputElement {
    return this.el('filter');
  }

  private findTrialRow(trial: Trial): TrialRow|undefined {
    for (const t of this.trials) {
      if (t.trial.hash === trial.hash) {
        return t;
      }
    }
    return undefined;
  }

  toggleForceEnable(trial: Trial, group: Group) {
    group.forceEnabled = !group.forceEnabled;
    const trialRow = this.findTrialRow(trial);
    if (trialRow) {
      for (const row of trialRow.experimentRows) {
        row.setForceEnabled(group.forceEnabled && row.group.hash == group.hash);
      }
    }

    this.proxy_.setTrialEnrollState(trial.hash, group.hash, group.forceEnabled);
    this.setRestartRequired_();
  }

  private populateState_(state: FieldTrialState) {
    const trialListDiv = this.el('field-trial-list');
    this.trials = state.trials.map(t => new TrialRow(this, t));
    this.trials.sort((a, b) => a.sortKey().localeCompare(b.sortKey()));
    trialListDiv.replaceChildren(...this.trials.map(t => t.root));
    this.dirty = true;
    if (state.restartRequired) {
      this.setRestartRequired_();
    }
    this.update_();
  }

  private filterToInput_(): void {
    this.filter_(this.filterInputElement.value);
  }

  private filter_(searchText: string): void {
    const searchFilter = new SearchFilter(this.unhasher, searchText);
    let matchGroupCount = 0;
    let matchTrialCount = 0;
    let totalExperimentCount = 0;
    for (const trial of this.trials) {
      const [matched, matchedGroups] = trial.filter(searchFilter);
      if (matched) {
        ++matchTrialCount;
      }
      matchGroupCount += matchedGroups;
      totalExperimentCount += trial.experimentRows.length;
    }
    // Expand all if the search term matches fewer than half of all experiment
    // groups.
    this.el('field-trial-list').dataset['expandAll'] = String(
        matchGroupCount > 0 && matchGroupCount < totalExperimentCount / 2);
    this.el('filter-status')
        .replaceChildren(
            ` (matched ${matchTrialCount} trials, ${matchGroupCount} groups)`);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'field-trials-app': FieldTrialsAppElement;
  }
}

customElements.define(FieldTrialsAppElement.is, FieldTrialsAppElement);
