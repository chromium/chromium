// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// eslint-disable-next-line no-unused-vars
import {browserProxy} from '../browser_proxy/browser_proxy.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from '../device/camera3_device_info.js';
import {
  PhotoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
  VideoConstraintsPreferrer,  // eslint-disable-line no-unused-vars
} from '../device/constraints_preferrer.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../device/device_info_updater.js';
import * as dom from '../dom.js';
import * as nav from '../nav.js';
import * as state from '../state.js';
import {
  Facing,
  Resolution,      // eslint-disable-line no-unused-vars
  ResolutionList,  // eslint-disable-line no-unused-vars
  ViewName,
} from '../type.js';
import * as util from '../util.js';

import {View} from './view.js';

/* eslint-disable no-unused-vars */

/**
 * Object of device id, preferred capture resolution and all
 * available resolutions for a particular video device.
 * @typedef {{prefResol: !Resolution, resols: !ResolutionList}}
 */
let ResolutionConfig;

/**
 * Photo and video resolution configuration for a particular video device.
 * @typedef {{
 *   deviceId: string,
 *   photo: !ResolutionConfig,
 *   video: !ResolutionConfig,
 * }}
 */
let DeviceSetting;

/* eslint-enable no-unused-vars */

/**
 * Base controller of settings view.
 */
export class BaseSettings extends View {
  /**
   * @param {!ViewName} name Name of the view.
   * @param {!Object<string, function(!Event=)>=} itemHandlers Click-handlers
   *     mapped by element ids.
   */
  constructor(name, itemHandlers = {}) {
    super(name, true, true);

    this.root.querySelector('.menu-header button')
        .addEventListener('click', () => this.leave());
    this.root.querySelectorAll('.menu-item').forEach((element) => {
      /** @type {function(!Event=)|undefined} */
      const handler = itemHandlers[element.id];
      if (handler) {
        element.addEventListener('click', handler);
      }
    });
  }

  /**
   * @override
   */
  focus() {
    this.rootElement_.querySelector('[tabindex]').focus();
  }

  /**
   * Opens sub-settings.
   * @param {!ViewName} name Name of settings view.
   * @private
   */
  openSubSettings(name) {
    // Dismiss primary-settings if sub-settings was dismissed by background
    // click.
    nav.open(name).then((cond) => cond && cond['bkgnd'] && this.leave(cond));
  }
}

/**
 * Controller of primary settings view.
 */
export class PrimarySettings extends BaseSettings {
  /**
   * @public
   */
  constructor() {
    super(ViewName.SETTINGS, {
      'settings-gridtype': () => this.openSubSettings(ViewName.GRID_SETTINGS),
      'settings-timerdur': () => this.openSubSettings(ViewName.TIMER_SETTINGS),
      'settings-resolution': () =>
          this.openSubSettings(ViewName.RESOLUTION_SETTINGS),
      'settings-expert': () => this.openSubSettings(ViewName.EXPERT_SETTINGS),
      'settings-feedback': () => {
        // Prevent setting view overlapping preview when sending app window
        // feedback screenshot b/155938542.
        this.leave();
        browserProxy.openFeedback();
      },
      'settings-help': () => util.openHelp(),
    });
  }
}

/**
 * Controller of resolution settings view.
 */
export class ResolutionSettings extends BaseSettings {
  /**
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {!PhotoConstraintsPreferrer} photoPreferrer
   * @param {!VideoConstraintsPreferrer} videoPreferrer
   */
  constructor(infoUpdater, photoPreferrer, videoPreferrer) {
    /**
     * @param {function(): ?DeviceSetting} getSetting
     * @param {function(): !HTMLElement} getElement
     * @param {boolean} isPhoto
     * @return {function()}
     */
    const createOpenMenuHandler = (getSetting, getElement, isPhoto) => () => {
      const setting = getSetting();
      if (setting === null) {
        console.error('Open settings of non-exist device.');
        return;
      }
      const element = getElement();
      if (element.classList.contains('multi-option')) {
        if (isPhoto) {
          this.openPhotoResSettings_(setting, element);
        } else {
          this.openVideoResSettings_(setting, element);
        }
      }
    };
    super(ViewName.RESOLUTION_SETTINGS, {
      'settings-front-photores': createOpenMenuHandler(
          () => this.frontSetting_, () => this.frontPhotoItem_, true),
      'settings-front-videores': createOpenMenuHandler(
          () => this.frontSetting_, () => this.frontVideoItem_, false),
      'settings-back-photores': createOpenMenuHandler(
          () => this.backSetting_, () => this.backPhotoItem_, true),
      'settings-back-videores': createOpenMenuHandler(
          () => this.backSetting_, () => this.backVideoItem_, false),
    });

    /**
     * @type {!PhotoConstraintsPreferrer}
     * @private
     */
    this.photoPreferrer_ = photoPreferrer;

    /**
     * @type {!VideoConstraintsPreferrer}
     * @private
     */
    this.videoPreferrer_ = videoPreferrer;

    /**
     * @const {!BaseSettings}
     * @public
     */
    this.photoResolutionSettings =
        new BaseSettings(ViewName.PHOTO_RESOLUTION_SETTINGS);

    /**
     * @const {!BaseSettings}
     * @public
     */
    this.videoResolutionSettings =
        new BaseSettings(ViewName.VIDEO_RESOLUTION_SETTINGS);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.resMenu_ = dom.getFrom(this.root, 'div.menu', HTMLDivElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.videoResMenu_ = dom.getFrom(
        this.videoResolutionSettings.root, 'div.menu', HTMLDivElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.photoResMenu_ = dom.getFrom(
        this.photoResolutionSettings.root, 'div.menu', HTMLDivElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.frontPhotoItem_ = dom.get('#settings-front-photores', HTMLElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.frontVideoItem_ = dom.get('#settings-front-videores', HTMLElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.backPhotoItem_ = dom.get('#settings-back-photores', HTMLElement);

    /**
     * @type {!HTMLElement}
     * @private
     */
    this.backVideoItem_ = dom.get('#settings-back-videores', HTMLElement);

    /**
     * Device setting of front camera. Null if no front camera.
     * @type {?DeviceSetting}
     * @private
     */
    this.frontSetting_ = null;

    /**
     * Device setting of back camera. Null if no front camera.
     * @type {?DeviceSetting}
     * @private
     */
    this.backSetting_ = null;

    /**
     * Device setting of external cameras.
     * @type {!Array<!DeviceSetting>}
     * @private
     */
    this.externalSettings_ = [];

    /**
     * Device id of currently opened resolution setting view.
     * @type {?string}
     * @private
     */
    this.openedSettingDeviceId_ = null;

    infoUpdater.addDeviceChangeListener(async (updater) => {
      /** @type {?Array<!Camera3DeviceInfo>} */
      const devices = await updater.getCamera3DevicesInfo();
      if (devices === null) {
        state.set(state.State.NO_RESOLUTION_SETTINGS, true);
        return;
      }

      this.frontSetting_ = this.backSetting_ = null;
      this.externalSettings_ = [];

      devices.forEach(({deviceId, facing}) => {
        const photoResols =
            this.photoPreferrer_.getSupportedResolutions(deviceId);
        const videoResols =
            this.videoPreferrer_.getSupportedResolutions(deviceId);
        const /** !DeviceSetting */ deviceSetting = {
          deviceId,
          photo: {
            prefResol: /** @type {!Resolution} */ (
                photoPreferrer.getPrefResolution(deviceId)),
            resols:
                /* Filter out resolutions of megapixels < 0.1 i.e. megapixels
                 * 0.0*/
                photoResols.filter((r) => r.area >= 100000),
          },
          video: {
            prefResol: /** @type {!Resolution} */ (
                videoPreferrer.getPrefResolution(deviceId)),
            resols: videoResols,
          },
        };
        switch (facing) {
          case Facing.USER:
            this.frontSetting_ = deviceSetting;
            break;
          case Facing.ENVIRONMENT:
            this.backSetting_ = deviceSetting;
            break;
          case Facing.EXTERNAL:
            this.externalSettings_.push(deviceSetting);
            break;
          default:
            console.error(`Ignore device of unknown facing: ${facing}`);
        }
      });
      this.updateResolutions_();
    });

    this.photoPreferrer_.setPreferredResolutionChangeListener(
        this.updateSelectedPhotoResolution_.bind(this));
    this.videoPreferrer_.setPreferredResolutionChangeListener(
        this.updateSelectedVideoResolution_.bind(this));

    // Flips 'disabled' of resolution options.
    [state.State.CAMERA_CONFIGURING, state.State.TAKING].forEach((s) => {
      state.addObserver(s, () => {
        dom.getAll('.resolution-option>input', HTMLInputElement)
            .forEach((e) => {
              e.disabled = state.get(state.State.CAMERA_CONFIGURING) ||
                  state.get(state.State.TAKING);
            });
      });
    });
  }

  /**
   * Template for generating option text from photo resolution width and height.
   * @param {!Resolution} r Resolution of text to be generated.
   * @param {!ResolutionList} resolutions All available resolutions.
   * @return {string} Text shown on resolution option item.
   * @private
   */
  photoOptTextTempl_(r, resolutions) {
    /**
     * @param {number} a
     * @param {number} b
     * @return {number}
     */
    const gcd = (a, b) => (a === 0 ? b : gcd(b % a, a));
    /**
     * @param {!Resolution} r
     * @return {number}
     */
    const toMegapixel = ({area}) =>
        area >= 1e6 ? Math.round(area / 1e6) : Math.round(area / 1e5) / 10;
    const /** number */ d = gcd(r.width, r.height);
    if (resolutions.some(
            (findR) => !findR.equals(r) && r.aspectRatioEquals(findR) &&
                toMegapixel(r) === toMegapixel(findR))) {
      return browserProxy.getI18nMessage(
          'label_detail_photo_resolution', r.width / d, r.height / d, r.width,
          r.height, toMegapixel(r));
    } else {
      return browserProxy.getI18nMessage(
          'label_photo_resolution', r.width / d, r.height / d, toMegapixel(r));
    }
  }

  /**
   * Template for generating option text from video resolution width and height.
   * @param {!Resolution} r Resolution of text to be generated.
   * @return {string} Text shown on resolution option item.
   * @private
   */
  videoOptTextTempl_(r) {
    return browserProxy.getI18nMessage(
        'label_video_resolution', r.height, r.width);
  }

  /**
   * Finds photo and video resolution setting of target device id.
   * @param {string} deviceId
   * @return {?DeviceSetting}
   * @private
   */
  getDeviceSetting_(deviceId) {
    if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
      return this.frontSetting_;
    }
    if (this.backSetting_ && this.backSetting_.deviceId === deviceId) {
      return this.backSetting_;
    }
    return this.externalSettings_.find((e) => e.deviceId === deviceId) || null;
  }

  /**
   * Updates resolution information of front, back camera and external cameras.
   * @private
   */
  updateResolutions_() {
    /**
     * @param {!HTMLElement} item
     * @param {string} id
     * @param {!ResolutionConfig} config
     * @param {function(!Resolution, !ResolutionList): string} optTextTempl
     */
    const prepItem = (item, id, {prefResol, resols}, optTextTempl) => {
      item.dataset['deviceId'] = id;
      item.classList.toggle('multi-option', resols.length > 1);
      item.querySelector('.description>span').textContent =
          optTextTempl(prefResol, resols);
    };

    // Update front camera setting
    state.set(state.State.HAS_FRONT_CAMERA, this.frontSetting_ !== null);
    if (this.frontSetting_) {
      const {deviceId, photo, video} = this.frontSetting_;
      prepItem(this.frontPhotoItem_, deviceId, photo, this.photoOptTextTempl_);
      prepItem(this.frontVideoItem_, deviceId, video, this.videoOptTextTempl_);
    }

    // Update back camera setting
    state.set(state.State.HAS_BACK_CAMERA, this.backSetting_ !== null);
    if (this.backSetting_) {
      const {deviceId, photo, video} = this.backSetting_;
      prepItem(this.backPhotoItem_, deviceId, photo, this.photoOptTextTempl_);
      prepItem(this.backVideoItem_, deviceId, video, this.videoOptTextTempl_);
    }

    // Update external camera settings
    // To prevent losing focus on item already exist before update, locate
    // focused item in both previous and current list, pop out all items in
    // previous list except those having same deviceId as focused one and
    // recreate all other items from current list.
    const prevFocus = /** @type {?HTMLElement} */ (
        this.resMenu_.querySelector('.menu-item.external-camera:focus'));
    /** @type {?string} */
    const prevFId = prevFocus && prevFocus.dataset['deviceId'];
    const /** number */ focusIdx =
        this.externalSettings_.findIndex(({deviceId}) => deviceId === prevFId);
    const fTitle = /** @type {?HTMLElement} */ (this.resMenu_.querySelector(
        `.external-camera.title-item[data-device-id="${prevFId}"]`));
    /** @type {?string} */
    const focusedId = focusIdx === -1 ? null : prevFId;

    dom.getAllFrom(this.resMenu_, '.menu-item.external-camera', HTMLElement)
        .forEach(
            (element) => element.dataset['deviceId'] !== focusedId &&
                element.parentNode.removeChild(element));

    this.externalSettings_.forEach((config, index) => {
      const {deviceId} = config;
      let /** !HTMLElement */ titleItem;
      let /** !HTMLElement */ photoItem;
      let /** !HTMLElement */ videoItem;
      if (deviceId !== focusedId) {
        const extItem =
            dom.instantiateTemplate('#extcam-resolution-item-template');
        [titleItem, photoItem, videoItem] =
            dom.getAllFrom(extItem, '.menu-item', HTMLElement);

        photoItem.addEventListener('click', () => {
          if (photoItem.classList.contains('multi-option')) {
            this.openPhotoResSettings_(config, photoItem);
          }
        });
        photoItem.setAttribute('aria-describedby', `${deviceId}-photores-desc`);
        photoItem.querySelector('.description').id =
            `${deviceId}-photores-desc`;
        videoItem.addEventListener('click', () => {
          if (videoItem.classList.contains('multi-option')) {
            this.openVideoResSettings_(config, videoItem);
          }
        });
        videoItem.setAttribute('aria-describedby', `${deviceId}-videores-desc`);
        videoItem.querySelector('.description').id =
            `${deviceId}-videores-desc`;
        if (index < focusIdx) {
          this.resMenu_.insertBefore(extItem, fTitle);
        } else {
          this.resMenu_.appendChild(extItem);
        }
      } else {
        titleItem = /** @type {!HTMLElement}*/ (fTitle);
        photoItem = /** @type {!HTMLElement}*/ (fTitle.nextElementSibling);
        videoItem = /** @type {!HTMLElement}*/ (photoItem.nextElementSibling);
      }
      titleItem.dataset['deviceId'] = deviceId;
      prepItem(photoItem, deviceId, config.photo, this.photoOptTextTempl_);
      prepItem(videoItem, deviceId, config.video, this.videoOptTextTempl_);
    });
    // Force closing opened setting of unplugged device.
    if ((state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) ||
         state.get(ViewName.VIDEO_RESOLUTION_SETTINGS)) &&
        this.openedSettingDeviceId_ !== null &&
        this.getDeviceSetting_(this.openedSettingDeviceId_) === null) {
      nav.close(
          state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) ?
              ViewName.PHOTO_RESOLUTION_SETTINGS :
              ViewName.VIDEO_RESOLUTION_SETTINGS);
    }
  }

  /**
   * Updates current selected photo resolution.
   * @param {string} deviceId Device id of the selected resolution.
   * @param {!Resolution} resolution Selected resolution.
   * @private
   */
  updateSelectedPhotoResolution_(deviceId, resolution) {
    const {photo} = this.getDeviceSetting_(deviceId);
    photo.prefResol = resolution;
    let /** !HTMLElement */ photoItem;
    if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
      photoItem = this.frontPhotoItem_;
    } else if (this.backSetting_ && this.backSetting_.deviceId === deviceId) {
      photoItem = this.backPhotoItem_;
    } else {
      photoItem = dom.getFrom(
          this.resMenu_, `.menu-item.photo-item[data-device-id="${deviceId}"]`,
          HTMLElement);
    }
    photoItem.querySelector('.description>span').textContent =
        this.photoOptTextTempl_(photo.prefResol, photo.resols);

    // Update setting option if it's opened.
    if (state.get(ViewName.PHOTO_RESOLUTION_SETTINGS) &&
        this.openedSettingDeviceId_ === deviceId) {
      const input = dom.getFrom(
          this.photoResMenu_,
          'input' +
              `[data-width="${resolution.width}"]` +
              `[data-height="${resolution.height}"]`,
          HTMLInputElement);
      input.checked = true;
    }
  }

  /**
   * Updates current selected video resolution.
   * @param {string} deviceId Device id of the selected resolution.
   * @param {!Resolution} resolution Selected resolution.
   * @private
   */
  updateSelectedVideoResolution_(deviceId, resolution) {
    const {video} = this.getDeviceSetting_(deviceId);
    video.prefResol = resolution;
    let /** !HTMLElement */ videoItem;
    if (this.frontSetting_ && this.frontSetting_.deviceId === deviceId) {
      videoItem = this.frontVideoItem_;
    } else if (this.backSetting_ && this.backSetting_.deviceId === deviceId) {
      videoItem = this.backVideoItem_;
    } else {
      videoItem = dom.getFrom(
          this.resMenu_, `.menu-item.video-item[data-device-id="${deviceId}"]`,
          HTMLElement);
    }
    videoItem.querySelector('.description>span').textContent =
        this.videoOptTextTempl_(video.prefResol);

    // Update setting option if it's opened.
    if (state.get(ViewName.VIDEO_RESOLUTION_SETTINGS) &&
        this.openedSettingDeviceId_ === deviceId) {
      const input = dom.getFrom(
          this.videoResMenu_,
          'input' +
              `[data-width="${resolution.width}"]` +
              `[data-height="${resolution.height}"]`,
          HTMLInputElement);
      input.checked = true;
    }
  }

  /**
   * Opens photo resolution setting view.
   * @param {!DeviceSetting} Setting of video device to be opened.
   * @param {!HTMLElement} resolItem Dom element from upper layer menu item
   *     showing title of the selected resolution.
   * @private
   */
  openPhotoResSettings_({deviceId, photo}, resolItem) {
    this.openedSettingDeviceId_ = deviceId;
    this.updateMenu_(
        resolItem, this.photoResMenu_, this.photoOptTextTempl_,
        (r) => this.photoPreferrer_.changePreferredResolution(deviceId, r),
        photo.resols, photo.prefResol);
    this.openSubSettings(ViewName.PHOTO_RESOLUTION_SETTINGS);
  }

  /**
   * Opens video resolution setting view.
   * @param {!DeviceSetting} Setting of video device to be opened.
   * @param {!HTMLElement} resolItem Dom element from upper layer menu item
   *     showing title of the selected resolution.
   * @private
   */
  openVideoResSettings_({deviceId, video}, resolItem) {
    this.openedSettingDeviceId_ = deviceId;
    this.updateMenu_(
        resolItem, this.videoResMenu_, this.videoOptTextTempl_,
        (r) => this.videoPreferrer_.changePreferredResolution(deviceId, r),
        video.resols, video.prefResol);
    this.openSubSettings(ViewName.VIDEO_RESOLUTION_SETTINGS);
  }

  /**
   * Updates resolution menu with specified resolutions.
   * @param {!HTMLElement} resolItem DOM element holding selected resolution.
   * @param {!HTMLElement} menu Menu holding all resolution option elements.
   * @param {function(!Resolution, !ResolutionList): string} optTextTempl
   *     Template generating text content for each resolution option from its
   *     width and height.
   * @param {function(!Resolution)} onChange Called when selected option
   *     changed with resolution of newly selected option.
   * @param {!ResolutionList} resolutions Resolutions of its width and height to
   *     be updated with.
   * @param {!Resolution} selectedR Selected resolution.
   * @private
   */
  updateMenu_(resolItem, menu, optTextTempl, onChange, resolutions, selectedR) {
    const captionText = resolItem.querySelector('.description>span');
    captionText.textContent = '';
    menu.querySelectorAll('.menu-item')
        .forEach((element) => element.parentNode.removeChild(element));

    resolutions.forEach((r) => {
      const item = dom.instantiateTemplate('#resolution-item-template');
      const label = dom.getFrom(item, 'label', HTMLLabelElement);
      util.setInkdropEffect(label);
      const input = dom.getFrom(item, 'input', HTMLInputElement);
      item.querySelector('span').textContent = optTextTempl(r, resolutions);
      input.name = menu.dataset['name'];
      input.dataset['width'] = r.width.toString();
      input.dataset['height'] = r.height.toString();
      if (r.equals(selectedR)) {
        captionText.textContent = optTextTempl(r, resolutions);
        input.checked = true;
      }
      input.disabled = state.get(state.State.CAMERA_CONFIGURING) ||
          state.get(state.State.TAKING);
      input.addEventListener('change', (event) => {
        if (input.checked) {
          captionText.textContent = optTextTempl(r, resolutions);
          onChange(r);
        }
      });
      menu.appendChild(item);
    });
  }
}
