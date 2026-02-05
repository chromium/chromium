// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SettingsDialog {
  static #instance = null;

  static getInstance() {
    return SettingsDialog.#instance ??
        (SettingsDialog.#instance = new SettingsDialog(
             $('settings-toggle'), $('settings-dialog'), $('dialog-backdrop'),
             $('theme-selection'), $('font-family-selection')));
  }

  constructor(
      toggleElement, dialogElement, backdropElement, themeFieldset,
      fontFamilySelect) {
    this._toggleElement = toggleElement;
    this._dialogElement = dialogElement;
    this._backdropElement = backdropElement;
    this._themeFieldset = themeFieldset;
    this._fontFamilySelect = fontFamilySelect;

    this._toggleElement.addEventListener('click', this.toggle.bind(this));
    this._dialogElement.addEventListener('close', this.close.bind(this));
    this._backdropElement.addEventListener('click', this.close.bind(this));

    $('close-settings-button').addEventListener('click', this.close.bind(this));

    this._themeFieldset.addEventListener('change', (e) => {
      const newTheme = e.target.value;
      this.useTheme(newTheme);
      distiller.storeThemePref(themeClasses.indexOf(newTheme));
    });

    this._fontFamilySelect.addEventListener('change', (e) => {
      const newFontFamily = e.target.value;
      this.useFontFamily(newFontFamily);
      distiller.storeFontFamilyPref(fontFamilyClasses.indexOf(newFontFamily));
    });

    // Appearance settings are loaded from user preferences, so on page load
    // the controllers for these settings may need to be updated to reflect
    // the active setting.
    this._updateFontFamilyControls(getAppearanceSetting(fontFamilyClasses));
    const selectedTheme = getAppearanceSetting(themeClasses);
    this._updateThemeControls(selectedTheme);
    updateToolbarColor(selectedTheme);
  }

  toggle() {
    if (this._dialogElement.open) {
      this.close();
    } else {
      this.showModal();
    }
  }

  showModal() {
    this._toggleElement.classList.add('activated');
    this._backdropElement.style.display = 'block';
    this._dialogElement.showModal();
  }

  close() {
    this._toggleElement.classList.remove('activated');
    this._backdropElement.style.display = 'none';
    this._dialogElement.close();
  }

  useTheme(theme) {
    themeClasses.forEach(
        (element) =>
            document.body.classList.toggle(element, element === theme));
    this._updateThemeControls(theme);
    updateToolbarColor(theme);
  }

  _updateThemeControls(theme) {
    const queryString = `input[value=${theme}]`;
    this._themeFieldset.querySelector(queryString).checked = true;
  }

  useFontFamily(fontFamily) {
    fontFamilyClasses.forEach(
        (element) =>
            document.body.classList.toggle(element, element === fontFamily));
    this._updateFontFamilyControls(fontFamily);
  }

  _updateFontFamilyControls(fontFamily) {
    this._fontFamilySelect.selectedIndex =
        fontFamilyClasses.indexOf(fontFamily);
  }
}
