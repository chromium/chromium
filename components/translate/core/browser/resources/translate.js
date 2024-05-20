// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is used in conjunction with the Google Translate Element script.
// It is executed in an isolated world of a page to translate it from one
// language to another.
// It should be included in the page before the Translate Element script.

// eslint-disable-next-line no-var
var cr = cr || {};

/**
 * An object to provide functions to interact with the Translate library.
 * @type {object}
 */
cr.googleTranslate = (function() {
  /**
   * The Translate Element library's instance.
   * @type {object}
   */
  let lib;

  /**
   * A flag representing if the Translate Element library is initialized.
   * @type {boolean}
   */
  let libReady = false;

  /**
   * Error definitions for |errorCode|. See chrome/common/translate_errors.h
   * to modify the definition.
   * @const
   */
  const ERROR = {
    'NONE': 0,
    'INITIALIZATION_ERROR': 2,
    'UNSUPPORTED_LANGUAGE': 4,
    'TRANSLATION_ERROR': 6,
    'TRANSLATION_TIMEOUT': 7,
    'UNEXPECTED_SCRIPT_ERROR': 8,
    'BAD_ORIGIN': 9,
    'SCRIPT_LOAD_ERROR': 10,
  };

  /**
   * Error code map from te.dom.DomTranslator.Error to |errorCode|.
   * See also go/dom_translator.js in google3.
   * @const
   */
  const TRANSLATE_ERROR_TO_ERROR_CODE_MAP = {
    0: ERROR['NONE'],
    1: ERROR['TRANSLATION_ERROR'],
    2: ERROR['UNSUPPORTED_LANGUAGE'],
  };

  /**
   * An error code happened in translate.js and the Translate Element library.
   */
  let errorCode = ERROR['NONE'];

  /**
   * A flag representing if the Translate Element has finished a translation.
   * @type {boolean}
   */
  let finished = false;

  /**
   * Counts how many times the checkLibReady function is called. The function
   * is called in every 100 msec and counted up to 6.
   * @type {number}
   */
  let checkReadyCount = 0;

  /**
   * Time in msec when this script is injected.
   * @type {number}
   */
  const injectedTime = performance.now();

  /**
   * Time in msec when the Translate Element library is loaded completely.
   * @type {number}
   */
  let loadedTime = 0.0;

  /**
   * Time in msec when the Translate Element library is initialized and ready
   * for performing translation.
   * @type {number}
   */
  let readyTime = 0.0;

  /**
   * Time in msec when the Translate Element library starts a translation.
   * @type {number}
   */
  let startTime = 0.0;

  /**
   * Time in msec when the Translate Element library ends a translation.
   * @type {number}
   */
  let endTime = 0.0;

  /**
   * Callback invoked when Translate Element's ready state is known.
   * Will only be invoked once to indicate successful or failed initialization.
   * In the failure case, errorCode() and error() will indicate the reason.
   * Only used on iOS.
   * @type {function}
   */
  let readyCallback;

  /**
   * Callback invoked when Translate Element's translation result is known.
   * Will only be invoked once to indicate successful or failed translation.
   * In the failure case, errorCode() and error() will indicate the reason.
   * Only used on iOS.
   * @type {function}
   */
  let resultCallback;

  function checkLibReady() {
    if (lib.isAvailable()) {
      readyTime = performance.now();
      libReady = true;
      invokeReadyCallback();
      return;
    }
    if (checkReadyCount++ > 5) {
      errorCode = ERROR['TRANSLATION_TIMEOUT'];
      invokeReadyCallback();
      return;
    }
    setTimeout(checkLibReady, 100);
  }

  function onTranslateProgress(progress, opt_finished, opt_error) {
    finished = opt_finished;
    // opt_error can be 'undefined'.
    if (typeof opt_error === 'boolean' && opt_error) {
      // TODO(toyoshim): Remove boolean case once a server is updated.
      errorCode = ERROR['TRANSLATION_ERROR'];
      // We failed to translate, restore so the page is in a consistent state.
      lib.restore();
      invokeResultCallback();
    } else if (typeof opt_error === 'number' && opt_error !== 0) {
      errorCode = TRANSLATE_ERROR_TO_ERROR_CODE_MAP[opt_error];
      lib.restore();
      invokeResultCallback();
    }
    // Translate works differently depending on the prescence of the native
    // IntersectionObserver APIs.
    // If it is available, translate will occur incrementally as the user
    // scrolls elements into view, and this method will be called continuously
    // with |opt_finished| always set as true.
    // On the other hand, if it is unavailable, the entire page will be
    // translated at once in a piece meal manner, and this method may still be
    // called several times, though only the last call will have |opt_finished|
    // set as true.
    if (finished) {
      endTime = performance.now();
      invokeResultCallback();
    }
  }

  function invokeReadyCallback() {
    if (readyCallback) {
      readyCallback();
      readyCallback = null;
    }
  }

  function invokeResultCallback() {
    if (resultCallback) {
      resultCallback();
      resultCallback = null;
    }
  }

  window.addEventListener('pagehide', function(event) {
    if (libReady && event.persisted) {
      lib.restore();
    }
  });

  // Public API.
  return {
    /**
     * Setter for readyCallback. No op if already set.
     * @param {function} callback The function to be invoked.
     */
    set readyCallback(callback) {
      if (!readyCallback) {
        readyCallback = callback;
      }
    },

    /**
     * Setter for resultCallback. No op if already set.
     * @param {function} callback The function to be invoked.
     */
    set resultCallback(callback) {
      if (!resultCallback) {
        resultCallback = callback;
      }
    },

    /**
     * Whether the library is ready.
     * The translate function should only be called when |libReady| is true.
     * @type {boolean}
     */
    get libReady() {
      return libReady;
    },

    /**
     * Whether the current translate has finished successfully.
     * @type {boolean}
     */
    get finished() {
      return finished;
    },

    /**
     * Whether an error occured initializing the library of translating the
     * page.
     * @type {boolean}
     */
    get error() {
      return errorCode !== ERROR['NONE'];
    },

    /**
     * Returns a number to represent error type.
     * @type {number}
     */
    get errorCode() {
      return errorCode;
    },

    /**
     * The language the page translated was in. Is valid only after the page
     * has been successfully translated and the original language specified to
     * the translate function was 'auto'. Is empty otherwise.
     * Some versions of Element library don't provide |getDetectedLanguage|
     * function. In that case, this function returns 'und'.
     * @type {boolean}
     */
    get sourceLang() {
      if (!libReady || !finished || errorCode !== ERROR['NONE']) {
        return '';
      }
      if (!lib.getDetectedLanguage) {
        return 'und';
      }  // Defined as translate::kUnknownLanguageCode in C++.
      return lib.getDetectedLanguage();
    },

    /**
     * Time in msec from this script being injected to all server side scripts
     * being loaded.
     * @type {number}
     */
    get loadTime() {
      if (loadedTime === 0) {
        return 0;
      }
      return loadedTime - injectedTime;
    },

    /**
     * Time in msec from this script being injected to the Translate Element
     * library being ready.
     * @type {number}
     */
    get readyTime() {
      if (!libReady) {
        return 0;
      }
      return readyTime - injectedTime;
    },

    /**
     * Time in msec to perform translation.
     * @type {number}
     */
    get translationTime() {
      if (!finished) {
        return 0;
      }
      return endTime - startTime;
    },

    /**
     * Translate the page contents.  Note that the translation is asynchronous.
     * You need to regularly check the state of |finished| and |errorCode| to
     * know if the translation finished or if there was an error.
     * @param {string} sourceLang The language the page is in.
     * @param {string} targetLang The language the page should be translated to.
     * @return {boolean} False if the translate library was not ready, in which
     *                   case the translation is not started.  True otherwise.
     */
    translate(sourceLang, targetLang) {
      finished = false;
      errorCode = ERROR['NONE'];
      if (!libReady) {
        return false;
      }
      startTime = performance.now();
      try {
        lib.translatePage(sourceLang, targetLang, onTranslateProgress);
      } catch (err) {
        console.error('Translate: ' + err);
        errorCode = ERROR['UNEXPECTED_SCRIPT_ERROR'];
        invokeResultCallback();
        return false;
      }
      return true;
    },

    /**
     * Reverts the page contents to its original value, effectively reverting
     * any performed translation.  Does nothing if the page was not translated.
     */
    revert() {
      lib.restore();
    },

    /**
     * Called when an error is caught while executing script fetched in
     * translate_script.cc.
     */
    onTranslateElementError(error) {
      errorCode = ERROR['UNEXPECTED_SCRIPT_ERROR'];
      invokeReadyCallback();
    },

    /**
     * Entry point called by the Translate Element once it has been injected in
     * the page.
     */
    onTranslateElementLoad() {
      loadedTime = performance.now();
      try {
        lib = google.translate.TranslateService({
          // translateApiKey is predefined by translate_script.cc.
          'key': translateApiKey,
          'serverParams': serverParams,
          'timeInfo': gtTimeInfo,
          'useSecureConnection': true,
        });
        translateApiKey = undefined;
        serverParams = undefined;
        gtTimeInfo = undefined;
      } catch (err) {
        errorCode = ERROR['INITIALIZATION_ERROR'];
        translateApiKey = undefined;
        serverParams = undefined;
        gtTimeInfo = undefined;
        invokeReadyCallback();
        return;
      }
      // The TranslateService is not available immediately as it needs to start
      // Flash.  Let's wait until it is ready.
      checkLibReady();
    },

    /**
     * Entry point called by the Translate Element when it want to load an
     * external CSS resource into the page.
     * @param {string} url URL of an external CSS resource to load.
     */
    onLoadCSS(url) {
      const element = document.createElement('link');
      element.type = 'text/css';
      element.rel = 'stylesheet';
      element.charset = 'UTF-8';
      element.href = url;
      document.head.appendChild(element);
    },

    /**
     * Entry point called by the Translate Element when it want to load and run
     * an external JavaScript on the page.
     * @param {string} url URL of an external JavaScript to load.
     */
    onLoadJavascript(url) {
      // securityOrigin is predefined by translate_script.cc.
      if (!url.startsWith(securityOrigin)) {
        console.error('Translate: ' + url + ' is not allowed to load.');
        errorCode = ERROR['BAD_ORIGIN'];
        return;
      }

      const xhr = new XMLHttpRequest();
      xhr.open('GET', url, true);
      xhr.onreadystatechange = function() {
        if (this.readyState !== this.DONE) {
          return;
        }
        if (this.status !== 200) {
          errorCode = ERROR['SCRIPT_LOAD_ERROR'];
          return;
        }
        // Execute translate script using an anonymous function on the window,
        // this prevents issues with the code being inside of the scope of the
        // XHR request.
        new Function(this.responseText).call(window);
      };
      xhr.send();
    },
  };
})();
