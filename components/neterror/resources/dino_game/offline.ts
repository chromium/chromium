// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {HIDDEN_CLASS} from '../constants.js';

import {DEFAULT_DIMENSIONS, FPS, IS_HIDPI, IS_IOS, IS_MOBILE, IS_RTL} from './constants.js';
import type {Dimensions} from './dimensions.js';
import {DistanceMeter} from './distance_meter.js';
import {GameOverPanel} from './game_over_panel.js';
import {GeneratedSoundFx} from './generated_sound_fx.js';
import {Horizon} from './horizon.js';
import type {Obstacle} from './obstacle.js';
import type {SpriteDefinition, SpriteDefinitionByType, SpritePositions} from './offline_sprite_definitions.js';
import {CollisionBox, GAME_TYPE, spriteDefinitionByType} from './offline_sprite_definitions.js';
import {Status as TrexStatus, Trex} from './trex.js';
import {getTimeStamp} from './utils.js';

enum A11yStrings {
  ARIA_LABEL = 'dinoGameA11yAriaLabel',
  DESCRIPTION = 'dinoGameA11yDescription',
  GAME_OVER = 'dinoGameA11yGameOver',
  HIGH_SCORE = 'dinoGameA11yHighScore',
  JUMP = 'dinoGameA11yJump',
  STARTED = 'dinoGameA11yStartGame',
  SPEED_LABEL = 'dinoGameA11ySpeedToggle',
}

/**
 * Default game configuration.
 * Shared config for all versions of the game. Additional parameters are
 * defined in GameModeConfig.
 */
interface BaseConfig {
  audiocueProximityThreshold: number;
  audiocueProximityThresholdMobileA11y: number;
  bgCloudSpeed: number;
  bottomPad: number;
  // Scroll Y threshold at which the game can be activated.
  canvasInViewOffset: number;
  clearTime: number;
  cloudFrequency: number;
  fadeDuration: number;
  flashDuration: number;
  gameoverClearTime: number;
  initialJumpVelocity: number;
  invertFadeDuration: number;
  maxBlinkCount: number;
  maxClouds: number;
  maxObstacleLength: number;
  maxObstacleDuplication: number;
  resourceTemplateId: string;
  speed: number;
  speedDropCoefficient: number;
  arcadeModeInitialTopPosition: number;
  arcadeModeTopPositionPercent: number;
}

interface GameModeConfig {
  acceleration: number;
  audiocueProximityThreshold: number;
  audiocueProximityThresholdMobileA11y: number;
  gapCoefficient: number;
  invertDistance: number;
  maxSpeed: number;
  mobileSpeedCoefficient: number;
  speed: number;
}

const defaultBaseConfig: BaseConfig = {
  audiocueProximityThreshold: 190,
  audiocueProximityThresholdMobileA11y: 250,
  bgCloudSpeed: 0.2,
  bottomPad: 10,
  // Scroll Y threshold at which the game can be activated.
  canvasInViewOffset: -10,
  clearTime: 3000,
  cloudFrequency: 0.5,
  fadeDuration: 1,
  flashDuration: 1000,
  gameoverClearTime: 1200,
  initialJumpVelocity: 12,
  invertFadeDuration: 12000,
  maxBlinkCount: 3,
  maxClouds: 6,
  maxObstacleLength: 3,
  maxObstacleDuplication: 2,
  resourceTemplateId: 'audio-resources',
  speed: 6,
  speedDropCoefficient: 3,
  arcadeModeInitialTopPosition: 35,
  arcadeModeTopPositionPercent: 0.1,
};

const normalModeConfig: GameModeConfig = {
  acceleration: 0.001,
  audiocueProximityThreshold: 190,
  audiocueProximityThresholdMobileA11y: 250,
  gapCoefficient: 0.6,
  invertDistance: 700,
  maxSpeed: 13,
  mobileSpeedCoefficient: 1.2,
  speed: 6,
};

const slowModeConfig: GameModeConfig = {
  acceleration: 0.0005,
  audiocueProximityThreshold: 170,
  audiocueProximityThresholdMobileA11y: 220,
  gapCoefficient: 0.3,
  invertDistance: 350,
  maxSpeed: 9,
  mobileSpeedCoefficient: 1.5,
  speed: 4.2,
};

type Config = BaseConfig&GameModeConfig;
type TrexDebugConfigSetting = 'gravity'|'minJumpHeight'|'speedDropCoefficient'|
    'initialJumpVelocity'|'speed';

/**
 * CSS class names.
 */
enum RunnerClasses {
  ARCADE_MODE = 'arcade-mode',
  CANVAS = 'runner-canvas',
  CONTAINER = 'runner-container',
  CRASHED = 'crashed',
  ICON = 'icon-offline',
  ICON_DISABLED = 'icon-disabled',
  INVERTED = 'inverted',
  SNACKBAR = 'snackbar',
  SNACKBAR_SHOW = 'snackbar-show',
  TOUCH_CONTROLLER = 'controller',
}

/**
 * Sound FX. Reference to the ID of the audio tag on interstitial page.
 */
enum RunnerSounds {
  BUTTON_PRESS = 'offline-sound-press',
  HIT = 'offline-sound-hit',
  SCORE = 'offline-sound-reached',
}

/**
 * Key code mapping.
 */
const runnerKeycodes: {jump: number[], duck: number[], restart: number[]} = {
  jump: [38, 32],  // Up, spacebar
  duck: [40],      // Down
  restart: [13],   // Enter
};

enum RunnerEvents {
  ANIM_END = 'webkitAnimationEnd',
  CLICK = 'click',
  KEYDOWN = 'keydown',
  KEYUP = 'keyup',
  POINTERDOWN = 'pointerdown',
  POINTERUP = 'pointerup',
  RESIZE = 'resize',
  TOUCHEND = 'touchend',
  TOUCHSTART = 'touchstart',
  VISIBILITY = 'visibilitychange',
  BLUR = 'blur',
  FOCUS = 'focus',
  LOAD = 'load',
  GAMEPADCONNECTED = 'gamepadconnected',
}

let runnerInstance: Runner|null = null;

const ARCADE_MODE_URL: string = 'chrome://dino/';

const RESOURCE_POSTFIX: string = 'offline-resources-';

/**
 * T-Rex runner.
 */
export class Runner {
  private outerContainerEl: HTMLElement;
  private containerEl: HTMLElement|null = null;
  // A div to intercept touch events. Only set while (playing && useTouch).
  private touchController: HTMLElement|null = null;
  private canvas: HTMLCanvasElement|null = null;
  private canvasCtx: CanvasRenderingContext2D|null = null;
  private a11yStatusEl: HTMLElement|null = null;
  private slowSpeedCheckboxLabel: HTMLElement|null = null;
  private slowSpeedCheckbox: HTMLInputElement|null = null;
  private slowSpeedToggleEl: HTMLElement|null = null;
  private origImageSprite: HTMLImageElement|null = null;
  private altCommonImageSprite: HTMLImageElement|null = null;
  private altGameImageSprite: HTMLImageElement|null = null;
  private imageSprite: HTMLImageElement|null = null;

  private config: Config;
  // Logical dimensions of the container.
  private dimensions: Dimensions = DEFAULT_DIMENSIONS;
  private gameType: (keyof SpriteDefinitionByType|null) = null;
  private spriteDefinition: SpriteDefinition = spriteDefinitionByType.original;
  private spriteDef: SpritePositions|null = null;

  // Alt game mode state.
  private altGameModeActive: boolean = false;
  private altGameModeFlashTimer: number|null = null;
  private altGameAssetsFailedToLoad: boolean = false;
  private fadeInTimer: number = 0;

  // UI components.
  private tRex: Trex|null = null;
  private distanceMeter: DistanceMeter|null = null;
  private gameOverPanel: GameOverPanel|null = null;
  private horizon: Horizon|null = null;

  private msPerFrame: number = 1000 / FPS;
  private time: number = 0;
  private distanceRan: number = 0;
  private runningTime: number = 0;
  private currentSpeed: number;
  private resizeTimerId?: number;
  private raqId: number = 0;
  private playCount: number = 0;

  // Whether the easter egg has been disabled. CrOS enterprise enrolled devices.
  private isDisabled: boolean = loadTimeData.valueExists('disabledEasterEgg');
  // Whether the easter egg has been activated.
  private activated: boolean = false;
  // Whether the game is currently in play state.
  private playing: boolean = false;
  private playingIntro: boolean = false;
  private crashed: boolean = false;
  private paused: boolean = false;
  private inverted: boolean = false;
  private isDarkMode: boolean = false;
  private updatePending: boolean = false;
  private hasSlowdownInternal: boolean = false;
  private hasAudioCuesInternal: boolean = false;

  private highestScore: number = 0;
  private syncHighestScore: boolean = false;

  private invertTimer: number = 0;
  private invertTrigger: boolean = false;

  private soundFx:
      Partial<{[K in keyof typeof RunnerSounds]: AudioBuffer}> = {};
  private audioContext: AudioContext|null = null;
  private generatedSoundFx: GeneratedSoundFx|null = null;

  // Gamepad state.
  private pollingGamepads: boolean = false;
  private gamepadIndex?: number;
  private previousGamepad: Gamepad|null = null;


  // Initialize the singleton instance of Runner. Should only be called once.
  static initializeInstance(outerContainerId: string, config?: Config): Runner {
    assert(runnerInstance === null);
    runnerInstance = new Runner(outerContainerId, config);
    runnerInstance.loadImages();

    return runnerInstance;
  }

  static getInstance(): Runner {
    assert(runnerInstance);
    return runnerInstance;
  }

  private constructor(outerContainerId: string, configParam?: Config) {
    const outerContainerElement =
        document.querySelector<HTMLElement>(outerContainerId);
    assert(outerContainerElement);
    this.outerContainerEl = outerContainerElement;

    this.config =
        configParam || Object.assign({}, defaultBaseConfig, normalModeConfig);

    this.currentSpeed = this.config.speed;

    if (this.isDisabled) {
      this.setupDisabledRunner();
      return;
    }

    if (this.isAltGameModeEnabled()) {
      this.initAltGameType();
    }

    window.initializeEasterEggHighScore = this.initializeHighScore.bind(this);
  }

  get hasSlowdown(): boolean {
    return this.hasSlowdownInternal;
  }

  get hasAudioCues(): boolean {
    return this.hasAudioCuesInternal;
  }

  /**
   * Whether an alternative game mode is enabled, returns true if the load time
   * data specifies it and its assets loaded successfully. Returns false
   * otherwise.
   */
  isAltGameModeEnabled(): boolean {
    if (this.altGameAssetsFailedToLoad) {
      return false;
    }
    return loadTimeData.valueExists('enableAltGameMode');
  }

  getGeneratedSoundFx(): GeneratedSoundFx {
    assert(this.generatedSoundFx);
    return this.generatedSoundFx;
  }

  getSpriteDefinition(): SpriteDefinition {
    return this.spriteDefinition;
  }

  getOrigImageSprite(): HTMLImageElement {
    assert(this.origImageSprite);
    return this.origImageSprite;
  }

  getRunnerImageSprite(): HTMLImageElement {
    assert(this.imageSprite);
    return this.imageSprite;
  }

  getRunnerAltGameImageSprite(): HTMLImageElement|null {
    return this.altGameImageSprite;
  }

  getAltCommonImageSprite(): HTMLImageElement|null {
    return this.altCommonImageSprite;
  }

  getConfig(): Config {
    return this.config;
  }

  /**
   * Initialize alternative game type.
   */
  private initAltGameType() {
    assert(loadTimeData.valueExists('altGameType'));
    if (GAME_TYPE.length > 0) {
      const parsedValue =
          Number.parseInt(loadTimeData.getValue('altGameType'), 10);
      const type = GAME_TYPE[parsedValue - 1];
      this.gameType = type || null;
    }
  }

  /**
   * For disabled instances, set up a snackbar with the disabled message.
   */
  private setupDisabledRunner() {
    this.containerEl = document.createElement('div');
    this.containerEl.className = RunnerClasses.SNACKBAR;
    this.containerEl.textContent = loadTimeData.getValue('disabledEasterEgg');
    this.outerContainerEl.appendChild(this.containerEl);

    // Show notification when the activation key is pressed.
    document.addEventListener(RunnerEvents.KEYDOWN, e => {
      if (runnerKeycodes.jump.includes(e.keyCode)) {
        assert(this.containerEl);
        this.containerEl.classList.add(RunnerClasses.SNACKBAR_SHOW);
        const iconElement = document.querySelector('.icon');
        assert(iconElement);
        iconElement.classList.add(RunnerClasses.ICON_DISABLED);
      }
    });
  }

  /**
   * Sets individual settings for debugging.
   */
  updateConfigSetting<K extends keyof Config>(setting: K, value: Config[K]) {
    this.config[setting] = value;
  }

  /**
   * Sets individual settings for debugging.
   */
  updateTrexConfigSetting(setting: TrexDebugConfigSetting, value: number) {
    assert(this.tRex);

    switch (setting) {
      case 'gravity':
      case 'minJumpHeight':
      case 'speedDropCoefficient':
        this.tRex.config[setting] = value;
        break;
      case 'initialJumpVelocity':
        this.tRex.setJumpVelocity(value);
        break;
      case 'speed':
        this.setSpeed(value);
        break;
      default:
        break;
    }
  }

  /**
   * Creates an on page image element from the base 64 encoded string source.
   * @param resourceName Name in data object,
   * @return The created element.
   */
  private createImageElement(resourceName: string): HTMLImageElement|null {
    const imgSrc = loadTimeData.valueExists(resourceName) ?
        loadTimeData.getString(resourceName) :
        null;

    if (imgSrc) {
      const el = document.createElement('img');
      el.id = resourceName;
      el.src = imgSrc;
      const resourcesElement = document.getElementById('offline-resources');
      assert(resourcesElement);
      resourcesElement.appendChild(el);
      return el;
    }
    return null;
  }

  /**
   * Cache the appropriate image sprite from the page and get the sprite sheet
   * definition.
   */
  private loadImages() {
    let scale = '1x';
    this.spriteDef = this.getSpriteDefinition().ldpi;
    if (IS_HIDPI) {
      scale = '2x';
      this.spriteDef = this.getSpriteDefinition().hdpi;
    }

    const imageSpriteElement = document.querySelector<HTMLImageElement>(
        `#${RESOURCE_POSTFIX + scale}`);
    assert(imageSpriteElement);
    this.imageSprite = imageSpriteElement;

    if (this.gameType) {
      this.altGameImageSprite =
          (this.createImageElement('altGameSpecificImage' + scale));
      this.altCommonImageSprite =
          (this.createImageElement('altGameCommonImage' + scale));
    }
    this.origImageSprite = this.getRunnerImageSprite();

    // Disable the alt game mode if the sprites can't be loaded.
    if (!this.getRunnerAltGameImageSprite() === null ||
        this.getAltCommonImageSprite() === null) {
      this.altGameAssetsFailedToLoad = true;
      this.altGameModeActive = false;
    }

    if (this.getRunnerImageSprite().complete) {
      this.init();
    } else {
      // If the images are not yet loaded, add a listener.
      this.getRunnerImageSprite().addEventListener(
          RunnerEvents.LOAD, this.init.bind(this));
    }
  }

  /**
   * Load and decode base 64 encoded sounds.
   */
  private loadSounds() {
    if (IS_IOS) {
      return;
    }
    this.audioContext = new AudioContext();

    const resourceTemplateElement = document.querySelector<HTMLTemplateElement>(
        `#${this.config.resourceTemplateId}`);
    assert(resourceTemplateElement);
    const resourceTemplate = resourceTemplateElement.content;

    for (const sound in RunnerSounds) {
      const audioElement = resourceTemplate.querySelector<HTMLAudioElement>(
          `#${RunnerSounds[sound as keyof typeof RunnerSounds]}`);
      assert(audioElement);
      let soundSrc = audioElement.src;
      soundSrc = soundSrc.substr(soundSrc.indexOf(',') + 1);
      const buffer = decodeBase64ToArrayBuffer(soundSrc);

      // Async, so no guarantee of order in array.
      this.audioContext.decodeAudioData(buffer, audioBuffer => {
        this.soundFx = {
          ...this.soundFx,
          [sound]: audioBuffer,
        };
      });
    }
  }

  /**
   * Sets the game speed. Adjust the speed accordingly if on a smaller screen.
   */
  private setSpeed(newSpeed?: number) {
    const speed = newSpeed || this.currentSpeed;

    // Reduce the speed on smaller mobile screens.
    if (this.dimensions.width < DEFAULT_DIMENSIONS.width) {
      const mobileSpeed = this.hasSlowdown ? speed :
                                             speed * this.dimensions.width /
              DEFAULT_DIMENSIONS.width * this.config.mobileSpeedCoefficient;
      this.currentSpeed = mobileSpeed > speed ? speed : mobileSpeed;
    } else if (newSpeed) {
      this.currentSpeed = newSpeed;
    }
  }

  /**
   * Game initialiser.
   */
  private init() {
    assert(this.spriteDef);
    const iconElement =
        document.querySelector<HTMLElement>('.' + RunnerClasses.ICON);
    assert(iconElement);

    // Hide the static icon.
    iconElement.style.visibility = 'hidden';

    if (this.isArcadeMode()) {
      document.title =
          document.title + ' - ' + getA11yString(A11yStrings.ARIA_LABEL);
    }

    this.adjustDimensions();
    this.setSpeed();

    const ariaLabel = getA11yString(A11yStrings.ARIA_LABEL);
    this.containerEl = document.createElement('div');
    this.containerEl.setAttribute('role', IS_MOBILE ? 'button' : 'application');
    this.containerEl.setAttribute('tabindex', '0');
    this.containerEl.setAttribute(
        'title', getA11yString(A11yStrings.DESCRIPTION));
    this.containerEl.setAttribute('aria-label', ariaLabel);

    this.containerEl.className = RunnerClasses.CONTAINER;

    // Player canvas container.
    this.canvas = createCanvas(
        this.containerEl, this.dimensions.width, this.dimensions.height);

    // Live region for game status updates.
    this.a11yStatusEl = document.createElement('span');
    this.a11yStatusEl.className = 'offline-runner-live-region';
    this.a11yStatusEl.setAttribute('aria-live', 'assertive');
    this.a11yStatusEl.textContent = '';

    // Add checkbox to slow down the game.
    this.slowSpeedCheckboxLabel = document.createElement('label');
    this.slowSpeedCheckboxLabel.className = 'slow-speed-option hidden';
    this.slowSpeedCheckboxLabel.textContent =
        getA11yString(A11yStrings.SPEED_LABEL);

    this.slowSpeedCheckbox = document.createElement('input');
    this.slowSpeedCheckbox.setAttribute('type', 'checkbox');
    this.slowSpeedCheckbox.setAttribute(
        'title', getA11yString(A11yStrings.SPEED_LABEL));
    this.slowSpeedCheckbox.setAttribute('tabindex', '0');
    this.slowSpeedCheckbox.setAttribute('checked', 'checked');

    this.slowSpeedToggleEl = document.createElement('span');
    this.slowSpeedToggleEl.className = 'slow-speed-toggle';

    this.slowSpeedCheckboxLabel.appendChild(this.slowSpeedCheckbox);
    this.slowSpeedCheckboxLabel.appendChild(this.slowSpeedToggleEl);

    if (IS_IOS) {
      this.outerContainerEl.appendChild(this.a11yStatusEl);
    } else {
      this.containerEl.appendChild(this.a11yStatusEl);
    }

    const canvasContext = this.canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.canvasCtx.fillStyle = '#f7f7f7';
    this.canvasCtx.fill();
    updateCanvasScaling(this.canvas);

    // Horizon contains clouds, obstacles and the ground.
    this.horizon = new Horizon(
        this.canvas, this.spriteDef, this.dimensions,
        this.config.gapCoefficient);

    // Distance meter
    this.distanceMeter = new DistanceMeter(
        this.canvas, this.spriteDef.textSprite, this.dimensions.width);

    // Draw t-rex
    this.tRex = new Trex(this.canvas, this.spriteDef.tRex);

    this.outerContainerEl.appendChild(this.containerEl);
    this.outerContainerEl.appendChild(this.slowSpeedCheckboxLabel);

    this.startListening();
    this.update();

    window.addEventListener(
        RunnerEvents.RESIZE, this.debounceResize.bind(this));

    // Handle dark mode
    const darkModeMediaQuery =
        window.matchMedia('(prefers-color-scheme: dark)');
    this.isDarkMode = darkModeMediaQuery && darkModeMediaQuery.matches;
    darkModeMediaQuery.addListener((e) => {
      this.isDarkMode = e.matches;
    });
  }

  /**
   * Create the touch controller. A div that covers whole screen.
   */
  private createTouchController() {
    this.touchController = document.createElement('div');
    this.touchController.className = RunnerClasses.TOUCH_CONTROLLER;
    this.touchController.addEventListener(RunnerEvents.TOUCHSTART, this);
    this.touchController.addEventListener(RunnerEvents.TOUCHEND, this);
    this.outerContainerEl.appendChild(this.touchController);
  }

  /**
   * Debounce the resize event.
   */
  private debounceResize() {
    if (this.resizeTimerId === undefined) {
      this.resizeTimerId = setInterval(this.adjustDimensions.bind(this), 250);
    }
  }

  /**
   * Adjust game space dimensions on resize.
   */
  private adjustDimensions() {
    clearInterval(this.resizeTimerId);
    this.resizeTimerId = undefined;

    const boxStyles = window.getComputedStyle(this.outerContainerEl);
    const padding = Number(
        boxStyles.paddingLeft.substr(0, boxStyles.paddingLeft.length - 2));

    this.dimensions.width = this.outerContainerEl.offsetWidth - padding * 2;
    if (this.isArcadeMode()) {
      this.dimensions.width =
          Math.min(DEFAULT_DIMENSIONS.width, this.dimensions.width);
      if (this.activated) {
        this.setArcadeModeContainerScale();
      }
    }

    // Redraw the elements back onto the canvas.
    if (this.canvas) {
      assert(this.distanceMeter);
      assert(this.horizon);
      assert(this.tRex);
      assert(this.containerEl);
      this.canvas.width = this.dimensions.width;
      this.canvas.height = this.dimensions.height;

      updateCanvasScaling(this.canvas);

      this.distanceMeter.calcXpos(this.dimensions.width);
      this.clearCanvas();
      this.horizon.update(0, 0, true, /*showNightMode = */ false);
      this.tRex.update(0);

      // Outer container and distance meter.
      if (this.playing || this.crashed || this.paused) {
        this.containerEl.style.width = this.dimensions.width + 'px';
        this.containerEl.style.height = this.dimensions.height + 'px';
        this.distanceMeter.update(0, Math.ceil(this.distanceRan));
        this.stop();
      } else {
        this.tRex.draw(0, 0);
      }

      // Game over panel.
      if (this.crashed && this.gameOverPanel) {
        this.gameOverPanel.updateDimensions(this.dimensions.width);
        this.gameOverPanel.draw(this.altGameModeActive, this.tRex);
      }
    }
  }

  /**
   * Play the game intro.
   * Canvas container width expands out to the full width.
   */
  private playIntro() {
    if (!this.activated && !this.crashed) {
      assert(this.tRex);
      assert(this.containerEl);
      this.playingIntro = true;
      this.tRex.playingIntro = true;

      // CSS animation definition.
      const keyframes = '@-webkit-keyframes intro { ' +
          'from { width:' + this.tRex.config.width + 'px }' +
          'to { width: ' + this.dimensions.width + 'px }' +
          '}';
      const styleSheet = document.styleSheets[0];
      assert(styleSheet);
      styleSheet.insertRule(keyframes, 0);

      this.containerEl.addEventListener(
          RunnerEvents.ANIM_END, this.startGame.bind(this));

      this.containerEl.style.webkitAnimation = 'intro .4s ease-out 1 both';
      this.containerEl.style.width = this.dimensions.width + 'px';

      this.setPlayStatus(true);
      this.activated = true;
    } else if (this.crashed) {
      this.restart();
    }
  }


  /**
   * Update the game status to started.
   */
  private startGame() {
    assert(this.containerEl);
    assert(this.tRex);
    if (this.isArcadeMode()) {
      this.setArcadeMode();
    }
    this.toggleSpeed();
    this.runningTime = 0;
    this.playingIntro = false;
    this.tRex.playingIntro = false;
    this.containerEl.style.webkitAnimation = '';
    this.playCount++;

    if (this.hasAudioCuesInternal) {
      this.getGeneratedSoundFx().background();
      this.containerEl.setAttribute('title', getA11yString(A11yStrings.JUMP));
    }

    // Handle tabbing off the page. Pause the current game.
    document.addEventListener(
        RunnerEvents.VISIBILITY, this.onVisibilityChange.bind(this));

    window.addEventListener(
        RunnerEvents.BLUR, this.onVisibilityChange.bind(this));

    window.addEventListener(
        RunnerEvents.FOCUS, this.onVisibilityChange.bind(this));
  }

  private clearCanvas() {
    assert(this.canvasCtx);
    this.canvasCtx.clearRect(
        0, 0, this.dimensions.width, this.dimensions.height);
  }

  /**
   * Checks whether the canvas area is in the viewport of the browser
   * through the current scroll position.
   */
  private isCanvasInView(): boolean {
    assert(this.containerEl);
    return this.containerEl.getBoundingClientRect().top >
        this.config.canvasInViewOffset;
  }

  /**
   * Enable the alt game mode. Switching out the sprites.
   */
  private enableAltGameMode() {
    this.imageSprite = this.getRunnerAltGameImageSprite();
    assert(this.gameType);
    assert(this.tRex);
    assert(this.horizon);
    this.spriteDefinition = spriteDefinitionByType[this.gameType];

    if (IS_HIDPI) {
      this.spriteDef = this.getSpriteDefinition().hdpi;
    } else {
      this.spriteDef = this.getSpriteDefinition().ldpi;
    }

    this.altGameModeActive = true;
    this.tRex.enableAltGameMode(this.spriteDef.tRex);
    this.horizon.enableAltGameMode(this.spriteDef);
    if (this.hasAudioCuesInternal) {
      this.getGeneratedSoundFx()?.background();
    }
  }

  /**
   * Update the game frame and schedules the next one.
   */
  private update() {
    assert(this.tRex);

    this.updatePending = false;

    const now = getTimeStamp();
    let deltaTime = now - (this.time || now);

    // Flashing when switching game modes.
    if (this.altGameModeFlashTimer !== null) {
      if (this.altGameModeFlashTimer <= 0) {
        this.altGameModeFlashTimer = null;
        this.tRex.setFlashing(false);
        this.enableAltGameMode();
      } else if (this.altGameModeFlashTimer > 0) {
        this.altGameModeFlashTimer -= deltaTime;
        this.tRex.update(deltaTime);
        deltaTime = 0;
      }
    }

    this.time = now;

    if (this.playing) {
      assert(this.distanceMeter);
      assert(this.horizon);
      assert(this.canvasCtx);

      this.clearCanvas();

      // Additional fade in - Prevents jump when switching sprites
      if (this.altGameModeActive &&
          this.fadeInTimer <= this.config.fadeDuration) {
        this.fadeInTimer += deltaTime / 1000;
        this.canvasCtx.globalAlpha = this.fadeInTimer;
      } else {
        this.canvasCtx.globalAlpha = 1;
      }

      if (this.tRex.jumping) {
        this.tRex.updateJump(deltaTime);
      }

      this.runningTime += deltaTime;
      const hasObstacles = this.runningTime > this.config.clearTime;

      // First jump triggers the intro.
      if (this.tRex.jumpCount === 1 && !this.playingIntro) {
        this.playIntro();
      }

      // The horizon doesn't move until the intro is over.
      if (this.playingIntro) {
        this.horizon.update(
            0, this.currentSpeed, hasObstacles, /* showNightMode = */ false);
      } else if (!this.crashed) {
        const showNightMode = this.isDarkMode !== this.inverted;
        deltaTime = !this.activated ? 0 : deltaTime;
        this.horizon.update(
            deltaTime, this.currentSpeed, hasObstacles, showNightMode);
      }

      const firstObstacle = this.horizon.obstacles[0];

      // Check for collisions.
      let collision = hasObstacles && firstObstacle &&
          this.checkForCollision(firstObstacle, this.tRex);

      // For a11y, audio cues.
      if (this.hasAudioCuesInternal && hasObstacles) {
        assert(firstObstacle);
        const jumpObstacle = firstObstacle.typeConfig.type !== 'collectable';

        if (!firstObstacle.jumpAlerted) {
          const threshold = this.config.audiocueProximityThreshold;
          const adjProximityThreshold = threshold +
              (threshold * Math.log10(this.currentSpeed / this.config.speed));

          if (firstObstacle.xPos < adjProximityThreshold) {
            if (jumpObstacle) {
              this.getGeneratedSoundFx().jump();
            }
            firstObstacle.jumpAlerted = true;
          }
        }
      }

      // Activated alt game mode.
      if (this.isAltGameModeEnabled() && collision && firstObstacle &&
          firstObstacle.typeConfig.type === 'collectable') {
        this.horizon.removeFirstObstacle();
        this.tRex.setFlashing(true);
        collision = false;
        this.altGameModeFlashTimer = this.config.flashDuration;
        this.runningTime = 0;
        if (this.hasAudioCuesInternal) {
          this.getGeneratedSoundFx().collect();
        }
      }

      if (!collision) {
        this.distanceRan += this.currentSpeed * deltaTime / this.msPerFrame;

        if (this.currentSpeed < this.config.maxSpeed) {
          this.currentSpeed += this.config.acceleration;
        }
      } else {
        this.gameOver();
      }

      const playAchievementSound =
          this.distanceMeter.update(deltaTime, Math.ceil(this.distanceRan));

      if (!this.hasAudioCuesInternal && playAchievementSound) {
        this.playSound(this.soundFx.SCORE);
      }

      // Night mode.
      if (!this.isAltGameModeEnabled()) {
        if (this.invertTimer > this.config.invertFadeDuration) {
          this.invertTimer = 0;
          this.invertTrigger = false;
          this.invert(false);
        } else if (this.invertTimer) {
          this.invertTimer += deltaTime;
        } else {
          const actualDistance =
              this.distanceMeter.getActualDistance(Math.ceil(this.distanceRan));

          if (actualDistance > 0) {
            this.invertTrigger = !(actualDistance % this.config.invertDistance);

            if (this.invertTrigger && this.invertTimer === 0) {
              this.invertTimer += deltaTime;
              this.invert(false);
            }
          }
        }
      }
    }

    if (this.playing ||
        (!this.activated && this.tRex.blinkCount < this.config.maxBlinkCount)) {
      this.tRex.update(deltaTime);
      this.scheduleNextUpdate();
    }
  }

  handleEvent(e: Event) {
    switch (e.type) {
      case RunnerEvents.KEYDOWN:
      case RunnerEvents.TOUCHSTART:
      case RunnerEvents.POINTERDOWN:
        this.onKeyDown(e);
        break;
      case RunnerEvents.KEYUP:
      case RunnerEvents.TOUCHEND:
      case RunnerEvents.POINTERUP:
        this.onKeyUp(e);
        break;
      case RunnerEvents.GAMEPADCONNECTED:
        this.onGamepadConnected();
        break;
      default:
    }
  }

  /**
   * Initialize audio cues if activated by focus on the canvas element.
   */
  private handleCanvasKeyPress(e: Event) {
    if (!this.activated && !this.hasAudioCuesInternal) {
      this.toggleSpeed();
      this.hasAudioCuesInternal = true;
      this.generatedSoundFx = new GeneratedSoundFx();
      this.config.clearTime *= 1.2;
    } else if (
        e instanceof KeyboardEvent && runnerKeycodes.jump.includes(e.keyCode)) {
      this.onKeyDown(e);
    }
  }

  /**
   * Prevent space key press from scrolling.
   */
  private preventScrolling(e: KeyboardEvent) {
    if (e.keyCode === 32) {
      e.preventDefault();
    }
  }

  /**
   * Toggle speed setting if toggle is shown.
   */
  private toggleSpeed() {
    if (this.hasAudioCuesInternal) {
      assert(this.slowSpeedCheckbox);
      const speedChange = this.hasSlowdown !== this.slowSpeedCheckbox.checked;

      if (speedChange) {
        assert(this.horizon);
        assert(this.tRex);
        this.hasSlowdownInternal = this.slowSpeedCheckbox.checked;
        const updatedConfig =
            this.hasSlowdown ? slowModeConfig : normalModeConfig;

        this.config = Object.assign(defaultBaseConfig, updatedConfig);
        this.currentSpeed = updatedConfig.speed;
        this.tRex.enableSlowConfig();
        this.horizon.adjustObstacleSpeed();
      }
      if (this.playing) {
        this.disableSpeedToggle(true);
      }
    }
  }

  /**
   * Show the speed toggle.
   * From focus event or when audio cues are activated.
   */
  private showSpeedToggle(e?: Event) {
    const isFocusEvent = e && e.type === 'focus';
    if (this.hasAudioCuesInternal || isFocusEvent) {
      assert(this.slowSpeedCheckboxLabel);
      this.slowSpeedCheckboxLabel.classList.toggle(
          HIDDEN_CLASS, isFocusEvent ? false : !this.crashed);
    }
  }

  /**
   * Disable the speed toggle.
   */
  private disableSpeedToggle(disable: boolean) {
    assert(this.slowSpeedCheckbox);
    if (disable) {
      this.slowSpeedCheckbox.setAttribute('disabled', 'disabled');
    } else {
      this.slowSpeedCheckbox.removeAttribute('disabled');
    }
  }

  /**
   * Bind relevant key / mouse / touch listeners.
   */
  private startListening() {
    assert(this.containerEl);
    assert(this.canvas);
    // A11y keyboard / screen reader activation.
    this.containerEl.addEventListener(
        RunnerEvents.KEYDOWN, this.handleCanvasKeyPress.bind(this));
    if (!IS_MOBILE) {
      this.containerEl.addEventListener(
          RunnerEvents.FOCUS, this.showSpeedToggle.bind(this));
    }
    this.canvas.addEventListener(
        RunnerEvents.KEYDOWN, this.preventScrolling.bind(this));
    this.canvas.addEventListener(
        RunnerEvents.KEYUP, this.preventScrolling.bind(this));

    // Keys.
    document.addEventListener(RunnerEvents.KEYDOWN, this);
    document.addEventListener(RunnerEvents.KEYUP, this);

    // Touch / pointer.
    this.containerEl.addEventListener(RunnerEvents.TOUCHSTART, this);
    document.addEventListener(RunnerEvents.POINTERDOWN, this);
    document.addEventListener(RunnerEvents.POINTERUP, this);

    if (this.isArcadeMode()) {
      // Gamepad
      window.addEventListener(RunnerEvents.GAMEPADCONNECTED, this);
    }
  }

  /**
   * Process keydown.
   */
  private onKeyDown(e: Event) {
    // Prevent native page scrolling whilst tapping on mobile.
    if (IS_MOBILE && this.playing) {
      e.preventDefault();
    }

    if (this.isCanvasInView()) {
      // Allow toggling of speed toggle.
      if (e instanceof KeyboardEvent &&
          runnerKeycodes.jump.includes(e.keyCode) &&
          e.target === this.slowSpeedCheckbox) {
        return;
      }

      if (!this.crashed && !this.paused) {
        // For a11y, screen reader activation.
        const isMobileMouseInput = IS_MOBILE && e instanceof PointerEvent &&
            e.type === RunnerEvents.POINTERDOWN && e.pointerType === 'mouse' &&
            (e.target === this.containerEl ||
             (IS_IOS &&
              (e.target === this.touchController || e.target === this.canvas)));
        assert(this.tRex);

        if ((e instanceof KeyboardEvent &&
             runnerKeycodes.jump.includes(e.keyCode)) ||
            e.type === RunnerEvents.TOUCHSTART || isMobileMouseInput) {
          e.preventDefault();
          // Starting the game for the first time.
          if (!this.playing) {
            // Started by touch so create a touch controller.
            if (!this.touchController && e.type === RunnerEvents.TOUCHSTART) {
              this.createTouchController();
            }

            if (isMobileMouseInput) {
              this.handleCanvasKeyPress(e);
            }
            this.loadSounds();
            this.setPlayStatus(true);
            this.update();
            if (window.errorPageController) {
              window.errorPageController.trackEasterEgg();
            }
          }
          // Start jump.
          if (!this.tRex.jumping && !this.tRex.ducking) {
            if (this.hasAudioCuesInternal) {
              this.getGeneratedSoundFx().cancelFootSteps();
            } else {
              this.playSound(this.soundFx.BUTTON_PRESS);
            }
            this.tRex.startJump(this.currentSpeed);
          }
        } else if (
            this.playing && e instanceof KeyboardEvent &&
            runnerKeycodes.duck.includes(e.keyCode)) {
          e.preventDefault();
          if (this.tRex.jumping) {
            // Speed drop, activated only when jump key is not pressed.
            this.tRex.setSpeedDrop();
          } else if (!this.tRex.jumping && !this.tRex.ducking) {
            // Duck.
            this.tRex.setDuck(true);
          }
        }
      }
    }
  }

  /**
   * Process key up.
   */
  private onKeyUp(e: Event) {
    assert(this.tRex);
    const keyCode = ('keyCode' in e) ? e.keyCode as number : 0;
    const isjumpKey = runnerKeycodes.jump.includes(keyCode) ||
        e.type === RunnerEvents.TOUCHEND || e.type === RunnerEvents.POINTERUP;

    if (this.isRunning() && isjumpKey) {
      this.tRex.endJump();
    } else if (runnerKeycodes.duck.includes(keyCode)) {
      this.tRex.speedDrop = false;
      this.tRex.setDuck(false);
    } else if (this.crashed) {
      // Check that enough time has elapsed before allowing jump key to restart.
      const deltaTime = getTimeStamp() - this.time;

      if (this.isCanvasInView() &&
          (runnerKeycodes.restart.includes(keyCode) ||
           this.isLeftClickOnCanvas(e) ||
           (deltaTime >= this.config.gameoverClearTime &&
            runnerKeycodes.jump.includes(keyCode)))) {
        this.handleGameOverClicks(e);
      }
    } else if (this.paused && isjumpKey) {
      // Reset the jump state
      this.tRex.reset();
      this.play();
    }
  }

  /**
   * Process gamepad connected event.
   */
  private onGamepadConnected() {
    if (!this.pollingGamepads) {
      this.pollGamepadState();
    }
  }

  /**
   * rAF loop for gamepad polling.
   */
  private pollGamepadState() {
    const gamepads: Array<Gamepad|null> = navigator.getGamepads();
    this.pollActiveGamepad(gamepads);

    this.pollingGamepads = true;
    requestAnimationFrame(this.pollGamepadState.bind(this));
  }

  /**
   * Polls for a gamepad with the jump button pressed. If one is found this
   * becomes the "active" gamepad and all others are ignored.
   */
  private pollForActiveGamepad(gamepads: Array<Gamepad|null>) {
    for (const [i, gamepad] of gamepads.entries()) {
      if (gamepad && gamepad.buttons.length > 0 &&
          gamepad.buttons[0]!.pressed) {
        this.gamepadIndex = i;
        this.pollActiveGamepad(gamepads);
        return;
      }
    }
  }

  /**
   * Polls the chosen gamepad for button presses and generates KeyboardEvents
   * to integrate with the rest of the game logic.
   */
  private pollActiveGamepad(gamepads: Array<Gamepad|null>) {
    if (this.gamepadIndex === undefined) {
      this.pollForActiveGamepad(gamepads);
      return;
    }

    const gamepad = gamepads[this.gamepadIndex];
    if (!gamepad) {
      this.gamepadIndex = undefined;
      this.pollForActiveGamepad(gamepads);
      return;
    }

    // The gamepad specification defines the typical mapping of physical buttons
    // to button indices: https://w3c.github.io/gamepad/#remapping
    this.pollGamepadButton(gamepad, 0, 38);  // Jump
    if (gamepad.buttons.length >= 2) {
      this.pollGamepadButton(gamepad, 1, 40);  // Duck
    }
    if (gamepad.buttons.length >= 10) {
      this.pollGamepadButton(gamepad, 9, 13);  // Restart
    }

    this.previousGamepad = gamepad;
  }

  /**
   * Generates a key event based on a gamepad button.
   */
  private pollGamepadButton(
      gamepad: Gamepad, buttonIndex: number, keyCode: number) {
    const state = gamepad.buttons[buttonIndex]?.pressed || false;
    let previousState = false;
    if (this.previousGamepad) {
      previousState =
          this.previousGamepad.buttons[buttonIndex]?.pressed || false;
    }
    // Generate key events on the rising and falling edge of a button press.
    if (state !== previousState) {
      const e = new KeyboardEvent(
          state ? RunnerEvents.KEYDOWN : RunnerEvents.KEYUP,
          {keyCode: keyCode});
      document.dispatchEvent(e);
    }
  }

  /**
   * Handle interactions on the game over screen state.
   * A user is able to tap the high score twice to reset it.
   */
  private handleGameOverClicks(e: Event) {
    if (e.target !== this.slowSpeedCheckbox) {
      assert(this.distanceMeter);
      e.preventDefault();
      if (this.distanceMeter.hasClickedOnHighScore(e) && this.highestScore) {
        if (this.distanceMeter.isHighScoreFlashing()) {
          // Subsequent click, reset the high score.
          this.saveHighScore(0, true);
          this.distanceMeter.resetHighScore();
        } else {
          // First click, flash the high score.
          this.distanceMeter.startHighScoreFlashing();
        }
      } else {
        this.distanceMeter.cancelHighScoreFlashing();
        this.restart();
      }
    }
  }

  /**
   * Returns whether the event was a left click on canvas.
   * On Windows right click is registered as a click.
   */
  private isLeftClickOnCanvas(e: Event): boolean {
    if (!(e instanceof MouseEvent)) {
      return false;
    }


    return e.button != null && e.button < 2 &&
        e.type === RunnerEvents.POINTERUP &&
        (e.target === this.canvas ||
         (IS_MOBILE && this.hasAudioCuesInternal &&
          e.target === this.containerEl));
  }

  /**
   * RequestAnimationFrame wrapper.
   */
  private scheduleNextUpdate() {
    if (!this.updatePending) {
      this.updatePending = true;
      this.raqId = requestAnimationFrame(this.update.bind(this));
    }
  }

  /**
   * Whether the game is running.
   */
  private isRunning(): boolean {
    return !!this.raqId;
  }

  /**
   * Set the initial high score as stored in the user's profile.
   */
  private initializeHighScore(highScore: number) {
    assert(this.distanceMeter);
    this.syncHighestScore = true;
    highScore = Math.ceil(highScore);
    if (highScore < this.highestScore) {
      if (window.errorPageController) {
        window.errorPageController.updateEasterEggHighScore(this.highestScore);
      }
      return;
    }
    this.highestScore = highScore;
    this.distanceMeter.setHighScore(this.highestScore);
  }

  /**
   * Sets the current high score and saves to the profile if available.
   * @param distanceRan Total distance ran.
   * @param  resetScore Whether to reset the score.
   */
  private saveHighScore(distanceRan: number, resetScore?: boolean) {
    assert(this.distanceMeter);
    this.highestScore = Math.ceil(distanceRan);
    this.distanceMeter.setHighScore(this.highestScore);

    // Store the new high score in the profile.
    if (this.syncHighestScore && window.errorPageController) {
      if (resetScore) {
        window.errorPageController.resetEasterEggHighScore();
      } else {
        window.errorPageController.updateEasterEggHighScore(this.highestScore);
      }
    }
  }

  /**
   * Game over state.
   */
  private gameOver() {
    assert(this.distanceMeter);
    assert(this.tRex);
    assert(this.containerEl);
    this.playSound(this.soundFx.HIT);
    vibrate(200);

    this.stop();
    this.crashed = true;
    this.distanceMeter.achievement = false;

    this.tRex.update(100, TrexStatus.CRASHED);

    // Game over panel.
    if (!this.gameOverPanel) {
      const origSpriteDef = IS_HIDPI ? spriteDefinitionByType.original.hdpi :
                                       spriteDefinitionByType.original.ldpi;

      if (this.canvas) {
        if (this.isAltGameModeEnabled()) {
          this.gameOverPanel = new GameOverPanel(
              this.canvas, origSpriteDef.textSprite, origSpriteDef.restart,
              this.dimensions, origSpriteDef.altGameEnd,
              this.altGameModeActive);
        } else {
          this.gameOverPanel = new GameOverPanel(
              this.canvas, origSpriteDef.textSprite, origSpriteDef.restart,
              this.dimensions);
        }
      }
    }

    assert(this.gameOverPanel);
    this.gameOverPanel.draw(this.altGameModeActive, this.tRex);

    // Update the high score.
    if (this.distanceRan > this.highestScore) {
      this.saveHighScore(this.distanceRan);
    }

    // Reset the time clock.
    this.time = getTimeStamp();

    if (this.hasAudioCuesInternal) {
      this.getGeneratedSoundFx().stopAll();
      assert(this.containerEl);
      this.announcePhrase(
          getA11yString(A11yStrings.GAME_OVER)
              .replace(
                  '$1',
                  this.distanceMeter.getActualDistance(this.distanceRan)
                      .toString()) +
          ' ' +
          getA11yString(A11yStrings.HIGH_SCORE)
              .replace(
                  '$1',

                  this.distanceMeter.getActualDistance(this.highestScore)
                      .toString()));
      this.containerEl.setAttribute(
          'title', getA11yString(A11yStrings.ARIA_LABEL));
    }
    this.showSpeedToggle();
    this.disableSpeedToggle(false);
  }

  private stop() {
    this.setPlayStatus(false);
    this.paused = true;
    cancelAnimationFrame(this.raqId);
    this.raqId = 0;
    if (this.hasAudioCuesInternal) {
      this.getGeneratedSoundFx().stopAll();
    }
  }

  private play() {
    if (!this.crashed) {
      assert(this.tRex);
      this.setPlayStatus(true);
      this.paused = false;
      this.tRex.update(0, TrexStatus.RUNNING);
      this.time = getTimeStamp();
      this.update();
      if (this.hasAudioCuesInternal) {
        this.getGeneratedSoundFx().background();
      }
    }
  }

  private restart() {
    if (!this.raqId) {
      assert(this.containerEl);
      assert(this.gameOverPanel);
      assert(this.tRex);
      assert(this.horizon);
      assert(this.distanceMeter);
      this.playCount++;
      this.runningTime = 0;
      this.setPlayStatus(true);
      this.toggleSpeed();
      this.paused = false;
      this.crashed = false;
      this.distanceRan = 0;
      this.setSpeed(this.config.speed);
      this.time = getTimeStamp();
      this.containerEl.classList.remove(RunnerClasses.CRASHED);
      this.clearCanvas();
      this.distanceMeter.reset();
      this.horizon.reset();
      this.tRex.reset();
      this.playSound(this.soundFx.BUTTON_PRESS);
      this.invert(true);
      this.update();
      this.gameOverPanel.reset();
      if (this.hasAudioCuesInternal) {
        this.getGeneratedSoundFx().background();
      }
      this.containerEl.setAttribute('title', getA11yString(A11yStrings.JUMP));
      this.announcePhrase(getA11yString(A11yStrings.STARTED));
    }
  }

  private setPlayStatus(isPlaying: boolean) {
    if (this.touchController) {
      this.touchController.classList.toggle(HIDDEN_CLASS, !isPlaying);
    }
    this.playing = isPlaying;
  }

  /**
   * Whether the game should go into arcade mode.
   */
  private isArcadeMode(): boolean {
    // In RTL languages the title is wrapped with the left to right mark
    // control characters &#x202A; and &#x202C but are invisible.
    return IS_RTL ? document.title.indexOf(ARCADE_MODE_URL) === 1 :
                    document.title === ARCADE_MODE_URL;
  }

  /**
   * Hides offline messaging for a fullscreen game only experience.
   */
  private setArcadeMode() {
    document.body.classList.add(RunnerClasses.ARCADE_MODE);
    this.setArcadeModeContainerScale();
  }

  /**
   * Sets the scaling for arcade mode.
   */
  private setArcadeModeContainerScale() {
    assert(this.containerEl);
    const windowHeight = window.innerHeight;
    const scaleHeight = windowHeight / this.dimensions.height;
    const scaleWidth = window.innerWidth / this.dimensions.width;
    const scale = Math.max(1, Math.min(scaleHeight, scaleWidth));
    const scaledCanvasHeight = this.dimensions.height * scale;
    // Positions the game container at 10% of the available vertical window
    // height minus the game container height.
    const translateY = Math.ceil(Math.max(
                           0,
                           (windowHeight - scaledCanvasHeight -
                            this.config.arcadeModeInitialTopPosition) *
                               this.config.arcadeModeTopPositionPercent)) *
        window.devicePixelRatio;

    const cssScale = IS_RTL ? -scale + ',' + scale : scale;
    this.containerEl.style.transform =
        'scale(' + cssScale + ') translateY(' + translateY + 'px)';
  }

  /**
   * Pause the game if the tab is not in focus.
   */
  private onVisibilityChange(e: Event) {
    if (document.hidden || e.type === 'blur' ||
        document.visibilityState !== 'visible') {
      this.stop();
    } else if (!this.crashed) {
      assert(this.tRex);
      this.tRex.reset();
      this.play();
    }
  }

  /**
   * Play a sound.
   */
  private playSound(soundBuffer?: AudioBuffer) {
    if (soundBuffer) {
      assert(this.audioContext);
      const sourceNode = this.audioContext.createBufferSource();
      sourceNode.buffer = soundBuffer;
      sourceNode.connect(this.audioContext.destination);
      sourceNode.start(0);
    }
  }

  /**
   * Inverts the current page / canvas colors.
   * @param reset Whether to reset colors.
   */
  private invert(reset: boolean) {
    const htmlEl = document.firstElementChild;
    assert(htmlEl);

    if (reset) {
      htmlEl.classList.toggle(RunnerClasses.INVERTED, false);
      this.invertTimer = 0;
      this.inverted = false;
    } else {
      this.inverted =
          htmlEl.classList.toggle(RunnerClasses.INVERTED, this.invertTrigger);
    }
  }

  /**
   * For screen readers make an announcement to the live region.
   * @param phrase Sentence to speak.
   */
  private announcePhrase(phrase: string) {
    if (this.a11yStatusEl) {
      this.a11yStatusEl.textContent = '';
      this.a11yStatusEl.textContent = phrase;
    }
  }

  /**
   * Check for a collision.
   * @param obstacle Obstacle object.
   * @param tRex T-rex object.
   * @param canvasCtx Optional canvas context for drawing collision boxes.
   */
  private checkForCollision(
      obstacle: Obstacle, tRex: Trex,
      canvasCtx?: CanvasRenderingContext2D): CollisionBox[]|null {
    // Adjustments are made to the bounding box as there is a 1 pixel white
    // border around the t-rex and obstacles.
    const tRexBox = new CollisionBox(
        tRex.xPos + 1, tRex.yPos + 1, tRex.config.width - 2,
        tRex.config.height - 2);

    const obstacleBox = new CollisionBox(
        obstacle.xPos + 1, obstacle.yPos + 1,
        obstacle.typeConfig.width * obstacle.size - 2,
        obstacle.typeConfig.height - 2);

    // Debug outer box
    if (canvasCtx) {
      drawCollisionBoxes(canvasCtx, tRexBox, obstacleBox);
    }

    // Simple outer bounds check.
    if (boxCompare(tRexBox, obstacleBox)) {
      const collisionBoxes = obstacle.collisionBoxes;
      let tRexCollisionBoxes: CollisionBox[] = [];

      if (this.isAltGameModeEnabled()) {
        const runnerSpriteDefinition = this.getSpriteDefinition();
        assert(runnerSpriteDefinition);
        assert(runnerSpriteDefinition.tRex);
        tRexCollisionBoxes = runnerSpriteDefinition.tRex.collisionBoxes;
      } else {
        tRexCollisionBoxes = tRex.getCollisionBoxes();
      }

      // Detailed axis aligned box check.
      for (const tRexCollisionBox of tRexCollisionBoxes) {
        for (const obstacleCollixionBox of collisionBoxes) {
          // Adjust the box to actual positions.
          const adjTrexBox =
              createAdjustedCollisionBox(tRexCollisionBox, tRexBox);
          const adjObstacleBox =
              createAdjustedCollisionBox(obstacleCollixionBox, obstacleBox);
          const crashed = boxCompare(adjTrexBox, adjObstacleBox);

          // Draw boxes for debug.
          if (canvasCtx) {
            drawCollisionBoxes(canvasCtx, adjTrexBox, adjObstacleBox);
          }

          if (crashed) {
            return [adjTrexBox, adjObstacleBox];
          }
        }
      }
    }

    return null;
  }
}


/**
 * Updates the canvas size taking into
 * account the backing store pixel ratio and
 * the device pixel ratio.
 *
 * See article by Paul Lewis:
 * http://www.html5rocks.com/en/tutorials/canvas/hidpi/
 *
 * @return Whether the canvas was scaled.
 */
function updateCanvasScaling(
    canvas: HTMLCanvasElement, width?: number, height?: number): boolean {
  const context = canvas.getContext('2d');
  assert(context);

  // Query the various pixel ratios
  const devicePixelRatio = Math.floor(window.devicePixelRatio) || 1;
  /** @suppress {missingProperties} */
  const backingStoreRatio = ('webkitBackingStorePixelRatio' in context) ?
      Math.floor(context.webkitBackingStorePixelRatio as number) :
      1;
  const ratio = devicePixelRatio / backingStoreRatio;

  // Upscale the canvas if the two ratios don't match
  if (devicePixelRatio !== backingStoreRatio) {
    const oldWidth = width || canvas.width;
    const oldHeight = height || canvas.height;

    canvas.width = oldWidth * ratio;
    canvas.height = oldHeight * ratio;

    canvas.style.width = oldWidth + 'px';
    canvas.style.height = oldHeight + 'px';

    // Scale the context to counter the fact that we've manually scaled
    // our canvas element.
    context.scale(ratio, ratio);
    return true;
  } else if (devicePixelRatio === 1) {
    // Reset the canvas width / height. Fixes scaling bug when the page is
    // zoomed and the devicePixelRatio changes accordingly.
    canvas.style.width = canvas.width + 'px';
    canvas.style.height = canvas.height + 'px';
  }
  return false;
}


/**
 * Returns a string from loadTimeData data object.
 */
function getA11yString(stringName: string): string {
  return loadTimeData.valueExists(stringName) ?
      loadTimeData.getString(stringName) :
      '';
}


/**
 * Vibrate on mobile devices.
 * @param duration Duration of the vibration in milliseconds.
 */
function vibrate(duration: number) {
  if (IS_MOBILE && window.navigator.vibrate) {
    window.navigator.vibrate(duration);
  }
}


/**
 * Create canvas element.
 * @param container Element to append canvas to.
 */
function createCanvas(
    container: Element, width: number, height: number,
    classname?: string): HTMLCanvasElement {
  const canvas = document.createElement('canvas');
  canvas.className =
      classname ? RunnerClasses.CANVAS + ' ' + classname : RunnerClasses.CANVAS;
  canvas.width = width;
  canvas.height = height;
  container.appendChild(canvas);

  return canvas;
}


/**
 * Decodes the base 64 audio to ArrayBuffer used by Web Audio.
 */
function decodeBase64ToArrayBuffer(base64String: string): ArrayBuffer {
  const len = (base64String.length / 4) * 3;
  const str = atob(base64String);
  const arrayBuffer = new ArrayBuffer(len);
  const bytes = new Uint8Array(arrayBuffer);

  for (let i = 0; i < len; i++) {
    bytes[i] = str.charCodeAt(i);
  }
  return bytes.buffer;
}



//******************************************************************************


/**
 * Adjust the collision box.
 * @param box The original box.
 * @param adjustment Adjustment box.
 * @return The adjusted collision box object.
 */
function createAdjustedCollisionBox(
    box: CollisionBox, adjustment: CollisionBox): CollisionBox {
  return new CollisionBox(
      box.x + adjustment.x, box.y + adjustment.y, box.width, box.height);
}


/**
 * Draw the collision boxes for debug.
 */
function drawCollisionBoxes(
    canvasCtx: CanvasRenderingContext2D, tRexBox: CollisionBox,
    obstacleBox: CollisionBox) {
  canvasCtx.save();
  canvasCtx.strokeStyle = '#f00';
  canvasCtx.strokeRect(tRexBox.x, tRexBox.y, tRexBox.width, tRexBox.height);

  canvasCtx.strokeStyle = '#0f0';
  canvasCtx.strokeRect(
      obstacleBox.x, obstacleBox.y, obstacleBox.width, obstacleBox.height);
  canvasCtx.restore();
}


/**
 * Compare two collision boxes for a collision.
 * @return Whether the boxes intersected.
 */
function boxCompare(tRexBox: CollisionBox, obstacleBox: CollisionBox): boolean {
  const tRexBoxX = tRexBox.x;
  const tRexBoxY = tRexBox.y;

  const obstacleBoxX = obstacleBox.x;
  const obstacleBoxY = obstacleBox.y;

  // Axis-Aligned Bounding Box method.
  if (tRexBoxX < obstacleBoxX + obstacleBox.width &&
      tRexBoxX + tRexBox.width > obstacleBoxX &&
      tRexBoxY < obstacleBoxY + obstacleBox.height &&
      tRexBox.height + tRexBoxY > obstacleBoxY) {
    return true;
  }

  return false;
}
