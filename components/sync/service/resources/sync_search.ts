// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {SyncNode, SyncNodeMap} from './chrome_sync.js';
import {getAllNodes, Timer} from './chrome_sync.js';

const ERROR_ATTR: string = 'error';
const SELECTED_ATTR: string = 'selected';

export class SyncSearchManager {
  private currSearchId_: number = 0;
  private resultsData_: object[] = [];
  private selected_: HTMLElement|null = null;
  private selectedIndex_: number = -1;
  private resultsControl_: HTMLElement;
  private detailsControl_: HTMLElement;
  private queryControl_: HTMLInputElement;
  private statusControl_: HTMLElement;

  /**
   * @param queryControl The <input> object of
   *     type=search where the user's query is typed.
   * @param submitControl The <button> object
   *     where the user can click to submit the query.
   * @param statusControl The <span> object display the
   *     search status.
   * @param resultsControl The <list> object which holds
   *     the list of returned results.
   * @param detailsControl The <pre> object which
   *     holds the details of the selected result.
   */
  constructor(
      queryControl: HTMLInputElement, submitControl: HTMLButtonElement,
      statusControl: HTMLElement, resultsControl: HTMLElement,
      detailsControl: HTMLElement) {
    this.resultsControl_ = resultsControl;
    this.detailsControl_ = detailsControl;
    this.queryControl_ = queryControl;
    this.statusControl_ = statusControl;

    submitControl.addEventListener('click', () => this.startSearch_());
    // Decorate search box.
    this.queryControl_.addEventListener('search', () => this.startSearch_());
    this.queryControl_.value = '';
    this.resultsControl_.setAttribute('role', 'list');
    this.resultsControl_.tabIndex = 0;
    this.resultsControl_.addEventListener(
        'keydown', e => this.handleKeydown_(e));
  }

  private startSearch_() {
    const query = this.queryControl_.value;
    this.statusControl_.textContent = '';
    this.resultsData_ = [];
    this.drawResultsList_();
    if (!query) {
      return;
    }

    this.statusControl_.textContent = 'Searching for ' + query + '...';
    this.queryControl_.toggleAttribute(ERROR_ATTR, false);
    this.doSearch_(query);
  }

  private setSelected_(newSelected: HTMLElement, newIndex: number) {
    if (this.selected_) {
      this.selected_.toggleAttribute(SELECTED_ATTR, false);
    }
    newSelected.toggleAttribute(SELECTED_ATTR, true);
    this.selected_ = newSelected;
    this.selectedIndex_ = newIndex;
    this.detailsControl_.textContent =
        JSON.stringify(this.resultsData_[this.selectedIndex_], null, 2);
  }

  private handleKeydown_(e: KeyboardEvent) {
    let newIndex: number = -1;
    if (e.key === 'ArrowUp') {
      newIndex = this.selectedIndex_ === -1 ?
          this.resultsData_.length - 1 :
          Math.min(0, this.selectedIndex_ - 1);
    } else if (
        e.key === 'ArrowDown' &&
        this.selectedIndex_ < this.resultsData_.length - 1) {
      newIndex = this.selectedIndex_ === -1 ? 0 : this.selectedIndex_ + 1;
    } else if (e.key === 'Home') {
      newIndex = 0;
    } else if (e.key === 'End') {
      newIndex = this.resultsData_.length - 1;
    }

    if (newIndex === -1) {
      return;
    }

    const items = this.resultsControl_.querySelectorAll('li');
    this.setSelected_(items[newIndex]!, newIndex);
    assert(this.selected_);
    this.selected_.scrollIntoViewIfNeeded();
    e.preventDefault();
  }

  private drawResultsList_() {
    this.selected_ = null;
    this.selectedIndex_ = -1;
    this.resultsControl_.innerHTML =
        window.trustedTypes ? window.trustedTypes.emptyHTML : '';
    this.resultsData_.forEach((item: object, index: number) => {
      const li = document.createElement('li');
      li.setAttribute('role', 'listitem');
      li.textContent = item.toString();
      this.resultsControl_.appendChild(li);
      li.addEventListener('click', () => {
        this.setSelected_(li, index);
      });
    });
  }

  /**
   * Runs a search with the given query.
   * @param query The regex to do the search with.
   */
  private doSearch_(query: string) {
    const timer = new Timer();
    this.currSearchId_++;
    const searchId = this.currSearchId_;
    try {
      const regex = new RegExp(query);
      getAllNodes((nodeMap: SyncNodeMap) => {
        // Put all nodes into one big list that ignores the type.
        const nodes: SyncNode[] =
            nodeMap.map(x => x.nodes).reduce((a, b) => a.concat(b));
        if (this.currSearchId_ !== searchId) {
          return;
        }
        this.displayResults_(
            timer, nodes.filter(function(elem) {
              return regex.test(JSON.stringify(elem, null, 2));
            }),
            null);
      });
    } catch (err) {
      // Sometimes the provided regex is invalid.  This and other errors will
      // be caught and handled here.
      this.displayResults_(timer, [], err as Error);
    }
  }

  private displayResults_(
      timer: Timer, nodes: Array<{NON_UNIQUE_NAME: string}>,
      error: Error|null) {
    if (error) {
      this.statusControl_.textContent = 'Error: ' + error;
      this.queryControl_.toggleAttribute(ERROR_ATTR, true);
    } else {
      this.statusControl_.textContent = 'Found ' + nodes.length + ' nodes in ' +
          timer.getElapsedSeconds() + 's';
      this.queryControl_.toggleAttribute(ERROR_ATTR, false);

      // TODO(akalin): Write a nicer list display.
      for (let i = 0; i < nodes.length; ++i) {
        nodes[i]!.toString = function() {
          return this.NON_UNIQUE_NAME;
        };
      }
      this.resultsData_.push(...nodes);
      this.drawResultsList_();
    }
  }

  setDataForTest(data: object[]) {
    this.resultsData_ = data;
    this.drawResultsList_();
  }
}

function createDoQueryFunction(
    queryControl: HTMLInputElement, submitControl: HTMLButtonElement,
    query: string): () => void {
  return function() {
    queryControl.value = query;
    submitControl.click();
  };
}

/**
 * Decorates the quick search controls
 *
 * @param quickLinkArray The <a> object which
 *     will be given a link to a quick filter option.
 * @param submitControl
 * @param queryControl The <input> object of type=search where user's query is
 *     typed.
 */
export function decorateQuickQueryControls(
    quickLinkArray: NodeListOf<HTMLElement>, submitControl: HTMLButtonElement,
    queryControl: HTMLInputElement) {
  for (let index = 0; index < quickLinkArray.length; ++index) {
    const quickQuery = quickLinkArray[index]!.getAttribute('data-query');
    assert(quickQuery);
    const quickQueryFunction =
        createDoQueryFunction(queryControl, submitControl, quickQuery);
    quickLinkArray[index]!.addEventListener('click', quickQueryFunction);
  }
}
