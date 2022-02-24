// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { Button as MwcButton } from '@material/mwc-button';
import { css, customElement, property, query, CSSResult, CSSResultArray } from 'lit-element';

function linearGradientOf(color : CSSResult) : CSSResult {
  return css`linear-gradient(${color}, ${color})`;
}

/**
 * A Google Material Design button, with modifications to match the Chrome OS
 * specs.
 */
@customElement('cros-button')
export class CrosButton extends MwcButton {
  constructor() {
    super();
    this.setAttribute('dir', document.dir);
    // Default to a secondary button.
    this.outlined = true;
    // Disable hover styling.
    this.rippleHandlers.startHover = () => {};
    this.rippleHandlers.endHover = () => {};
    // Disable Focus styling.
    this.rippleHandlers.startFocus = () => {};
    this.rippleHandlers.endFocus = () => {};
  }

  @property({type: Boolean, reflect: true, attribute: true})
  primary: boolean = false;

  @property({type: Boolean, reflect: true, attribute: true})
  text: boolean = false;

  @property({type: Boolean, reflect: true, attribute: true})
  pill: boolean = false;

  @property({type: Boolean, reflect: true, attribute: 'hide-label'})
  hideLabel: boolean = false;

  @property({type: String, reflect: true, attribute: 'aria-expanded'})
  ariaExpanded?: string;

  @query('button') htmlButton?: HTMLButtonElement;

  updateAriaLabels() {
    if (this.ariaExpanded !== undefined) {
      this.htmlButton!.setAttribute('aria-expanded', this.ariaExpanded);
      this.htmlButton!.setAttribute('aria-haspopup', 'true');
    } else {
      this.htmlButton!.removeAttribute('aria-expanded');
      this.htmlButton!.removeAttribute('aria-popup');
    }
  }

  firstUpdated() {
    // Add in initial aria labels.
    this.updateAriaLabels();
  }

  updated() {
    // Enforce that both primary and text styles can not be applied at the same
    // time.
    if (this.primary && this.text) {
      throw new Error("Primary/secondary/text modes are mutually exclusive");
    }
    this.unelevated = this.primary;
    this.outlined = !this.primary && !this.text;
    this.updateAriaLabels();
  }

  static getStyles(): CSSResultArray {
    const crosStyles = css`
      :host {
        /* Public API */
        --border-end-radius: 4px;
        --border-start-radius: 4px;
        --button-height: 32px;
        /* This only affects alignment in multiline labels. */
        --label-text-align: center;
        --vertical-padding: 0;

        /* Local variables */
        --active-shadow-color-key: var(--cros-button-active-shadow-color-key-secondary);
        --active-shadow-color-ambient: var(--cros-button-active-shadow-color-ambient-secondary);
        --active-shadow: 0 1px 2px 0 var(--active-shadow-color-key),
                         0 1px 3px 0 var(--active-shadow-color-ambient);

        --hover-color: var(--cros-button-background-color-secondary-hover);

        /* disabled */
        --mdc-button-disabled-fill-color: var(--cros-button-background-color-primary-disabled);
        --mdc-button-disabled-ink-color:  var(--cros-button-label-color-primary-disabled);
        --mdc-button-disabled-outline-color: var(--cros-button-background-color-primary-disabled);

        /* secondary */
        --mdc-ripple-press-opacity: var(--cros-button-secondary-ripple-opacity);
        --mdc-button-outline-color: var(--cros-button-stroke-color-secondary);
        --mdc-ripple-color: var(--cros-button-ripple-color-secondary);

        /* primary */
        --mdc-theme-primary: var(--cros-button-background-color-primary);
        --mdc-theme-on-primary: var(--cros-button-label-color-primary);

        /* typography */
        --mdc-typography-button-font-size: 13px;
        --mdc-typography-button-font-weight: 500;
        --mdc-typography-button-letter-spacing: normal;
        --mdc-typography-button-text-transform: none;
        line-height: 20px;
      }

      :host([primary]) {
        --active-shadow-color-key: var(--cros-button-active-shadow-color-key-primary);
        --active-shadow-color-ambient: var(--cros-button-active-shadow-color-ambient-primary);
        --mdc-ripple-press-opacity: var(--cros-button-primary-ripple-opacity);
        --mdc-ripple-color: var(--cros-button-ripple-color-primary);
        --hover-color: var(--cros-button-background-color-primary-hover);
      }

      :host([pill]) {
        --border-start-radius: 16px;
        --border-end-radius: 16px;
      }

      :host button {
        height: var(--button-height);
        min-height: 32px;
        min-width: 32px;
        border-radius: var(--border-start-radius) var(--border-end-radius)
                       var(--border-end-radius) var(--border-start-radius);
      }

      :host([dir="rtl"]) button {
        border-radius: var(--border-end-radius) var(--border-start-radius)
                       var(--border-start-radius) var(--border-end-radius);
      }

      /*
       * TODO(b/155822587): Remove this rule when vertical density is
       * implemented for mwc-button.
       */
      :host .mdc-button--outlined {
        /*
         * For outlined button, subtract the outlined border width, i.e. 1px as
         * defined in
         * https://source.corp.google.com/piper///depot/google3/third_party/javascript/material_components_web/button/_variables.scss?l=41&rcl=289657622.
         */
        padding-bottom: max(calc(var(--vertical-padding) - 1px), 0px);
        padding-top: max(calc(var(--vertical-padding) - 1px), 0px);
      }
      :host .mdc-button--unelevated {
        padding-bottom: var(--vertical-padding);
        padding-top: var(--vertical-padding);
      }
      :host .mdc-button__label {
        text-align: var(--label-text-align);
      }

      :host([hide-label]) .mdc-button__label {
        display: none;
      }

      /*
       * The .hover/.active classes are only used by tests to simulate these
       * events.
       */
      :host button:hover, :host button.hover {
        --mdc-button-outline-color: var(--cros-button-stroke-color-secondary-hover);
        /*
         * Apply hover colors with background-image / linear-gradient to
         * blend with background colour at runtime and avoid pre-blending.
         */
        background-image: ${linearGradientOf(css`var(--hover-color)`)};
      }

      /* TODO(calamity): Specs say this should be a 2px border 2px away from the button. */
      :host button:focus-visible {
        outline: none;
        box-shadow: 0 0 0 2px var(--cros-focus-ring-color);
      }

      :host button:active, :host button.active {
        box-shadow: var(--active-shadow);
      }
      :host([text]) button:active, :host([text]) button.active {
        box-shadow: none;
      }
    `;
    return [MwcButton.styles, crosStyles];
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cros-button': CrosButton;
  }
}

