// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {RENDERER_ID_NOT_SET} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {isFormControlElement} from '//components/autofill/ios/form_util/resources/fill_element_inference_util.js';
import {getUniqueID} from '//components/autofill/ios/form_util/resources/fill_util.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage, trim} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * Retrieves the registered 'autofill_form_features' CrWebApi
 * instance for use in this file.
 */
// TODO: crbug.com/448990422 - Remove all utility functions
// from the gCrWeb object.
const autofillFormFeaturesApi =
    gCrWeb.getRegisteredApi('autofill_form_features');

/**
 * Prefix used in references to form elements that have no 'id' or 'name'
 */
const kNamelessFormIDPrefix = 'gChrome~form~';

/**
 * Prefix used in references to field elements that have no 'id' or 'name' but
 * are included in a form.
 */
const kNamelessFieldIDPrefix = 'gChrome~field~';

/**
 * Maps an Element to its position index under a specific tag.
 */
type TagIndexCache = WeakMap<Element, number>;

/**
 * Maps tag names (e.g., 'INPUT') to their corresponding element index
 * cache.
 */
type AncestorTagCache = Map<string, TagIndexCache>;

/**
 * Scoped cache mapping ancestors to their child element index cache.
 */
type ElementIndexCache = WeakMap<Element|ParentNode, AncestorTagCache>;

/**
 * Transient cache for element index by tag name lookup, cleared automatically
 * at the end of the current microtask.
 */
let elementIndexCache: ElementIndexCache|null = null;

/**
 * Returns true if autofill optimization form search is enabled.
 */
function isAutofillOptimizationFormSearchEnabled(): boolean {
  return (window as any).gCrWebPlaceholderAutofillOptimizationFormSearch;
}

/**
 * Returns the form's `name` attribute if non-empty; otherwise the form's `id`
 * attribute, or the index of the form (with prefix) in document.forms.
 *
 * It is partially based on the logic in
 *     const string16 GetFormIdentifier(const blink::WebFormElement& form)
 * in chromium/src/components/autofill/renderer/form_autofill_util.h.
 *
 * @param form An element for which the identifier is returned.
 * @return a string that represents the element's identifier.
 */
export function getFormIdentifier(form: Element|null): string {
  if (!form) {
    return '';
  }

  let name = form.getAttribute('name');
  if (name && name.length !== 0 &&
      form.ownerDocument.forms.namedItem(name) === form) {
    return name;
  }
  name = form.getAttribute('id');
  if (name && name.length !== 0 &&
      form.ownerDocument.getElementById(name) === form) {
    return name;
  }
  // A form name must be supplied, because the element will later need to be
  // identified from the name. A last resort is to take the index number of
  // the form in document.forms. ids are not supposed to begin with digits (by
  // HTML 4 spec) so this is unlikely to match a true id.
  for (let idx = 0; idx !== document.forms.length; idx++) {
    if (document.forms[idx] === form) {
      return kNamelessFormIDPrefix + idx;
    }
  }
  return '';
}

/**
 * Returns the form element from an ID obtained from getFormIdentifier.
 *
 * This works on a 'best effort' basis since DOM changes can always change the
 * actual element that the ID refers to.
 *
 * @param name An ID string obtained via getFormIdentifier.
 * @return The original form element, if it can be determined.
 */
export function getFormElementFromIdentifier(name: string): HTMLFormElement|
    null {
  // First attempt is from the name / id supplied.
  const form = document.forms.namedItem(name);
  if (form) {
    return form.nodeType === Node.ELEMENT_NODE ? form : null;
  }
  // Second attempt is from the prefixed index position of the form in
  // document.forms.
  if (name.indexOf(kNamelessFormIDPrefix) === 0) {
    const nameAsInteger =
        0 | name.substring(kNamelessFieldIDPrefix.length).length;
    if (kNamelessFormIDPrefix + nameAsInteger === name &&
        nameAsInteger < document.forms.length) {
      const form = document.forms[nameAsInteger];
      return form ? form : null;
    }
  }
  return null;
}

/**
 * Returns an array of control elements in a form.
 *
 * This method is based on the logic in method
 *     void WebFormElement::getFormControlElements(
 *         WebVector<WebFormControlElement>&) const
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/src/
 * WebFormElement.cpp.
 *
 * @param form A form element for which the control elements are returned.
 * @return An array of form control elements.
 */
export function getFormControlElements(form: HTMLFormElement|null): Element[] {
  if (!form) {
    return [];
  }
  const results: Element[] = [];
  // Get input and select elements from form.elements.
  // According to
  // http://www.w3.org/TR/2011/WD-html5-20110525/forms.html, form.elements are
  // the "listed elements whose form owner is the form element, with the
  // exception of input elements whose type attribute is in the Image Button
  // state, which must, for historical reasons, be excluded from this
  // particular collection." In WebFormElement.cpp, this method is implemented
  // by returning elements in form's associated elements that have tag 'INPUT'
  // or 'SELECT'. Check if input Image Buttons are excluded in that
  // implementation. Note for Autofill, as input Image Button is not
  // considered as autofillable elements, there is no impact on Autofill
  // feature.
  for (const element of form.elements) {
    if (isFormControlElement(element)) {
      results.push(element);
    }
  }
  return results;
}

/**
 * Returns an array of iframe elements that are descendents of `root`.
 *
 * @param root The node under which to search for iframe elements.
 * @return An array of iframe elements.
 */
export function getIframeElements(root: Element|null): HTMLIFrameElement[] {
  return Array.from(root?.querySelectorAll('iframe') ?? []) as
      HTMLIFrameElement[];
}

/**
 * Resolves the index of an `element` under a specific `ancestor` for its tag
 * name using a transient cache.
 */
function getElementIndexByTagName(
    ancestor: ParentNode, element: Element): number {
  if (!elementIndexCache) {
    elementIndexCache = new WeakMap();
    // Automatically clear the cache at the end of the current synchronous
    // execution block.
    queueMicrotask(() => {
      elementIndexCache = null;
    });
  }

  let ancestorCache = elementIndexCache.get(ancestor);
  if (!ancestorCache) {
    ancestorCache = new Map();
    elementIndexCache.set(ancestor, ancestorCache);
  }

  // Cache the index of each element with the given tag name under the ancestor.
  const tagName = element.tagName;
  let tagCache = ancestorCache.get(tagName);
  if (!tagCache) {
    tagCache = new WeakMap();
    const descendants = ancestor.querySelectorAll(tagName);
    for (let i = 0; i < descendants.length; i++) {
      tagCache.set(descendants[i] as Element, i);
    }
    ancestorCache.set(tagName, tagCache);
  }

  return tagCache.get(element) ?? -1;
}

/**
 * Returns the field's `id` attribute if not space only; otherwise the
 * form's |name| attribute if the field is part of a form. Otherwise,
 * generate a technical identifier
 *
 * It is the identifier that should be used for the specified |element| when
 * storing Autofill data. This identifier will be used when filling the field
 * to lookup this field. The pair (getFormIdentifier, getFieldIdentifier) must
 * be unique on the page.
 * The following elements are considered to generate the identifier:
 * - the id of the element
 * - the name of the element if the element is part of a form
 * - the order of the element in the form if the element is part of the form.
 * - generate a xpath to the element and use it as an ID.
 *
 * Note: if this method returns '', the field will not be accessible and
 * cannot be autofilled.
 *
 * It aims to provide the logic in
 *     WebString nameForAutofill() const;
 * in chromium/src/third_party/WebKit/Source/WebKit/chromium/public/
 *  WebFormControlElement.h
 *
 * @param element An element of which the name for Autofill will be
 *     returned.
 * @return the name for Autofill.
 */
export function getFieldIdentifier(element: Element|null): string {
  if (!element) {
    return '';
  }
  let trimmedIdentifier: string|null = element.id;
  if (trimmedIdentifier) {
    return trim(trimmedIdentifier);
  }
  if ('form' in element && element.form) {
    const form = element.form as HTMLFormElement;
    // The name of an element is only relevant as an identifier if the element
    // is part of a form.
    trimmedIdentifier = 'name' in element ? element.name as string : null;
    if (trimmedIdentifier) {
      trimmedIdentifier = trim(trimmedIdentifier);
      if (trimmedIdentifier!.length > 0) {
        return trimmedIdentifier!;
      }
    }

    const elements = getFormControlElements(form);
    for (let index = 0; index < elements.length; index++) {
      if (elements[index] === element) {
        return kNamelessFieldIDPrefix + index;
      }
    }
  }
  // Element is not part of a form and has no name or id, or usable attribute.
  // As best effort, try to find the closest ancestor with an id, then
  // check the index of the element in the descendants of the ancestors with
  // the same type.
  let ancestor: ParentNode|null = element.parentNode;
  while (ancestor && ancestor instanceof Element &&
         (!ancestor.hasAttribute('id') || trim(ancestor.id) === '')) {
    ancestor = ancestor.parentNode;
  }

  let ancestorId = '';
  if (!ancestor || !(ancestor instanceof Element)) {
    ancestor = document.body;
  }
  if (ancestor instanceof Element && ancestor.hasAttribute('id')) {
    ancestorId = '#' + trim(ancestor.id);
  }
  if (isAutofillOptimizationFormSearchEnabled()) {
    const index = getElementIndexByTagName(ancestor, element);
    if (index !== -1) {
      return `${kNamelessFieldIDPrefix}${ancestorId}~${element.tagName}~${
          index}`;
    }
  } else {
    const descendants = ancestor.querySelectorAll(element.tagName);
    for (let idx = 0; idx < descendants.length; idx++) {
      if (descendants[idx] === element) {
        return `${kNamelessFieldIDPrefix}${ancestorId}~${element.tagName}~${
            idx}`;
      }
    }
  }

  return '';
}

/**
 * Returns the form element from an form renderer id.
 *
 * @param identifier An ID string obtained via getFormIdentifier.
 * @return The original form element, if it can be determined.
 */
export function getFormElementFromRendererId(identifier: number):
    HTMLFormElement|null {
  if (identifier.toString() === RENDERER_ID_NOT_SET) {
    return null;
  }
  for (const form of document.forms) {
    if (identifier.toString() === getUniqueID(form)) {
      return form;
    }
  }
  return null;
}

// LINT.IfChange(autofill_count_form_submission_in_renderer)
// The source that triggered the sending of the form submission report.
enum FormSubmissionReportSource {
  // Report was sent immediately because quota was available.
  INSTANT,
  // Report was sent from the scheduled task.
  SCHEDULED_TASK,
  // Report was sent from unloading the page content.
  UNLOAD_PAGE,
}
// LINT.ThenChange(//components/autofill/ios/form_util/form_activity_tab_helper.mm:autofill_count_form_submission_in_renderer)


/**
 * Represent the number of form submissions split by type.
 */
interface FormSubmissionCountReport {
  // From a submit event.
  htmlEvent: number;
  // Triggered via `form.submit()`.
  programmatic: number;
}

/**
 * Manager of form submission reports. Takes care of throttling form submission
 * reports via quota and schedules batches of aggregated reports.
 */
class FormSubmissionReportManager {
  /**
   * Time period for refreshing the report quota.
   */
  private static readonly QUOTA_REFRESH_PERIOD_MS = 4000;  // 4 seconds

  /**
   * Time period in milliseconds between each form submission count report.
   */
  private static readonly REPORT_PERIOD_MS = 2000;  // 2 seconds

  /**
   * Number of reports allowed by the quota.
   */
  private static readonly QUOTA_SIZE = 2;

  // Maps the message handler to the pending reports to send to that handler.
  private formSubmissionCountReportMap: Map<string, FormSubmissionCountReport> =
      new Map();

  /**
   * Quota of form submission reports that can be sent before using throttling.
   * Reports sent under the quota are sent directly to the browser without
   * the need for scheduling a report which is much faster and reliable.
   */
  private formSubmissionReportQuotaRemaining =
      FormSubmissionReportManager.QUOTA_SIZE;

  constructor() {
    window.addEventListener('unload', () => {
      // Send the submission count report right now as the document is about to
      // be unloaded, meaning that the reporting scheduled task is likely to be
      // cancelled. This doesn't work when the entire tab is closed.
      this.sendFormSubmissionCountReports(
          FormSubmissionReportSource.UNLOAD_PAGE);
    });
  }

  sendReport(isProgrammatic: boolean, handler: string): void {
    if (!autofillFormFeaturesApi.getFunction(
            'isAutofillCountFormSubmissionInRendererEnabled')()) {
      // Do not report anything if the feature is disabled.
      return;
    }

    const scheduleReport = this.formSubmissionCountReportMap.size === 0;

    // Initialize the report if there isn't already one for the `handler`.
    if (!this.formSubmissionCountReportMap.has(handler)) {
      this.formSubmissionCountReportMap.set(
          handler, {htmlEvent: 0, programmatic: 0});
    }

    const report: FormSubmissionCountReport =
        this.formSubmissionCountReportMap.get(handler)!;

    if (isProgrammatic) {
      ++report.programmatic;
    } else {
      ++report.htmlEvent;
    }

    if (this.formSubmissionReportQuotaRemaining > 0) {
      --this.formSubmissionReportQuotaRemaining;
      // Report right away if the quota wasn't reached yet.
      this.sendFormSubmissionCountReports(FormSubmissionReportSource.INSTANT);
      // Reset the quota after a cooldown period.
      setTimeout(
          () => ++this.formSubmissionReportQuotaRemaining,
          FormSubmissionReportManager.QUOTA_REFRESH_PERIOD_MS);
      return;
    }

    if (scheduleReport) {
      // If no quota is available, schedule a report if there isn't already
      // one pending.
      const reportFn = () => this.sendFormSubmissionCountReports(
          FormSubmissionReportSource.SCHEDULED_TASK);
      setTimeout(reportFn, FormSubmissionReportManager.REPORT_PERIOD_MS);
    }
  }

  /**
   * Sends the `formSubmissionCountReport` (if there is) to the browser.
   */
  private sendFormSubmissionCountReports(source: FormSubmissionReportSource):
      void {
    this.formSubmissionCountReportMap.forEach(
        (report: FormSubmissionCountReport, handler: string) => {
          const message = {
            command: 'form.submit.count',
            ...report,
            source,
          };
          sendWebKitMessage(handler, message);
        });

    this.formSubmissionCountReportMap.clear();
  }
}

const gFormSubmissionReportManager = new FormSubmissionReportManager();

/**
 * Reports periodically (as needed) the form submission counts that were
 * detected before doing any processing. The count for each type of event is
 * provided (regular or programmatic).
 * @param isProgrammatic True if the source of the form submission is
 *   programmatic (i.e. comes from the prototype override).
 * @param handler Name of the browser handler to send the message with the count
 *   report to.
 */
export function reportDetectedFormSubmission(
    isProgrammatic: boolean, handler: string): void {
  // Ignore reporting if there is an error as this isn't a critical function.
  // Reporting form submission shouldn't have a side effect on processing the
  // form submission.
  try {
    gFormSubmissionReportManager.sendReport(isProgrammatic, handler);
  } catch {
    // Ignore.
  }
}

/**
 * Checks if the potential ancestor establishes a containing block for the
 * descendant.
 *
 * For detailed rules on what constitutes a containing block, see:
 * https://developer.mozilla.org/en-US/docs/Web/CSS/Guides/Display/Containing_block
 *
 * @param ancestorStyle The computed style of the potential containing block
 *     ancestor.
 * @param descendantStyle The computed style of the descendant element.
 * @return True if the ancestor is a containing block for the descendant.
 */
function establishesContainingBlockFor(
    ancestorStyle: CSSStyleDeclaration,
    descendantStyle: CSSStyleDeclaration): boolean {
  // TODO(crbug.com/532608141): Support Shadow DOM by crossing shadow
  // boundaries.

  // If the ancestor does not generate a box, it cannot be a containing block.
  if (ancestorStyle.display === 'contents') {
    return false;
  }

  const descendantPosition = descendantStyle.position;

  // For static, relative, or sticky elements, the containing block is
  // normally the immediate ancestor container.
  if (descendantPosition !== 'fixed' && descendantPosition !== 'absolute') {
    return true;
  }

  // For absolute positioned elements, any non-static ancestor traps them.
  if (descendantPosition === 'absolute' &&
      ancestorStyle.position !== 'static') {
    return true;
  }

  // At this point, the descendant is either `fixed`, OR `absolute` passing
  // through a `static` ancestor. Both are trapped by the exact same properties
  // below.

  // Transform, perspective, filter, backdrop-filter, or independent transforms.
  if (ancestorStyle.transform !== 'none' ||
      ancestorStyle.perspective !== 'none' || ancestorStyle.filter !== 'none' ||
      ancestorStyle.translate !== 'none' || ancestorStyle.rotate !== 'none' ||
      ancestorStyle.scale !== 'none' ||
      ancestorStyle.backdropFilter !== 'none') {
    return true;
  }
  const webkitBackdropFilter =
      ancestorStyle.getPropertyValue('-webkit-backdrop-filter');
  if (webkitBackdropFilter && webkitBackdropFilter !== 'none') {
    return true;
  }

  // CSS containment: layout, paint, strict, content, or content-visibility.
  const containValue = ancestorStyle.contain;
  const contentVisibility =
      ancestorStyle.getPropertyValue('content-visibility');
  if ((containValue &&
       /\b(layout|paint|strict|content)\b/.test(containValue)) ||
      contentVisibility === 'auto' || contentVisibility === 'hidden') {
    return true;
  }

  // CSS will-change properties that would create a containing block upfront.
  const willChange = ancestorStyle.willChange;
  if (willChange &&
      /\b(transform|perspective|filter|backdrop-filter|contain|translate|rotate|scale)\b/
          .test(willChange)) {
    return true;
  }

  // Ancestors with clip-path or mask also act as containing blocks for our
  // traversal because they clip all descendants regardless of positioning.
  const clipPath = ancestorStyle.getPropertyValue('clip-path');
  const maskImage = ancestorStyle.getPropertyValue('mask-image') ||
      ancestorStyle.getPropertyValue('-webkit-mask-image');
  if ((clipPath && clipPath !== 'none') ||
      (maskImage && maskImage !== 'none')) {
    return true;
  }

  return false;
}

/**
 * Clips the given rectangle with the clip box bounds.
 *
 * @param rect The rectangle to clip.
 * @param clipBox The clipping boundaries.
 * @return The clipped rectangle, or null if the result has no area.
 */
export function clipRect(
    rect: DOMRectReadOnly,
    clipBox: {left: number, top: number, right: number, bottom: number}):
    DOMRectReadOnly|null {
  const left = Math.max(rect.left, clipBox.left);
  const right = Math.min(rect.right, clipBox.right);
  const top = Math.max(rect.top, clipBox.top);
  const bottom = Math.min(rect.bottom, clipBox.bottom);

  // If the clipped rectangle has no area (width or height is 0 or negative),
  // it is completely clipped out.
  if (bottom <= top || right <= left) {
    return null;
  }
  return new DOMRect(left, top, right - left, bottom - top);
}

/**
 * Calculates the simulated scroll offset needed to bring a range defined by
 * start and length coordinates into a clipping boundary range.
 *
 * @param start The start coordinate of the element (e.g., rect.top).
 * @param length The length of the element (e.g., rect.height).
 * @param clipStart The start of the clipping container (e.g., clipTop).
 * @param clipEnd The end of the clipping container (e.g., clipBottom).
 * @param scrollLength The total scrollable content length (e.g., scrollHeight).
 * @param clientLength The viewport client length (e.g., clientHeight).
 * @param currentScroll The current scroll position (e.g., scrollTop).
 * @return The simulated scroll offset adjustment.
 */
function calculateSimulatedScrollOffset(
    start: number, length: number, clipStart: number, clipEnd: number,
    scrollLength: number, clientLength: number, currentScroll: number): number {
  const diffBefore = start - clipStart;
  const diffAfter = (start + length) - clipEnd;
  let scrollDiff = 0;

  // Checks scroll direction.
  if (diffBefore < 0) {
    scrollDiff = diffBefore;
  } else if (diffAfter > 0) {
    scrollDiff = diffAfter;
  }

  if (scrollDiff === 0) {
    return 0;
  }

  const maxScroll = scrollLength - clientLength;
  const targetScroll =
      Math.max(0, Math.min(maxScroll, currentScroll + scrollDiff));
  return targetScroll - currentScroll;
}

/**
 * Clips the given bounding rectangle against the boundaries of a clipping
 * element, taking the element's CSS overflow styling into account.
 *
 * @param rect The bounding rectangle to clip.
 * @param clippingElement The element that might clip the rectangle.
 * @param shouldAdjustRectForScroll Whether to simulate scroll adjustments on
 *     scrollable ancestors.
 * @return The clipped bounding rectangle, or null if it is completely
 *     clipped out.
 */
function clipRectWithElement(
    rect: DOMRectReadOnly, clippingElement: Element,
    shouldAdjustRectForScroll: boolean): DOMRectReadOnly|null {
  const isRoot = clippingElement ===
      (document.scrollingElement || document.documentElement);
  const style = window.getComputedStyle(clippingElement);
  const clipPath = style.getPropertyValue('clip-path');
  const maskImage = style.getPropertyValue('mask-image') ||
      style.getPropertyValue('-webkit-mask-image');

  const hasClipPath = !!clipPath && clipPath !== 'none';
  const hasMask = !!maskImage && maskImage !== 'none';

  const overflowX = style.overflowX;
  const overflowY = style.overflowY;

  // Viewport always clips. Otherwise, clip if overflow is not visible.
  const clipsX = isRoot || (overflowX !== 'visible' || hasClipPath || hasMask);
  const clipsY = isRoot || (overflowY !== 'visible' || hasClipPath || hasMask);

  // If the container doesn't clip overflow, and has no clip-path/mask, return
  // the rect untouched.
  if (!clipsX && !clipsY) {
    return rect;
  }

  // Viewport clipping boundary differs from normal elements:
  // - For standard DOM elements, clipping occurs at their padding-box boundary,
  //   which is computed relative to the current viewport using
  //   `getBoundingClientRect()` and adding the border widths
  //   (`clientLeft`/`clientTop`).
  // - For the root element (`document.documentElement`),
  // `getBoundingClientRect()`
  //   returns the total layout box of the document, which shifts and grows with
  //   content and is not aligned with the window viewport.
  const clipBox = clippingElement.getBoundingClientRect();
  const clipLeft = isRoot ? 0 : clipBox.left + clippingElement.clientLeft;
  const clipTop = isRoot ? 0 : clipBox.top + clippingElement.clientTop;
  const clipRight = isRoot ?
      (window.innerWidth || clippingElement.clientWidth) :
      clipLeft + clippingElement.clientWidth;
  const clipBottom = isRoot ?
      (window.innerHeight || clippingElement.clientHeight) :
      clipTop + clippingElement.clientHeight;

  // Adjusts the `rect` along scrollable directions to simulate user scrolling
  // to reveal the element.
  if (shouldAdjustRectForScroll) {
    let adjustedLeft = rect.left;
    let adjustedTop = rect.top;

    const scrollableMatcher = isRoot ? /^(?!hidden$)/ : /^(?:auto|scroll)$/;

    // Adjust for vertical scroll direction.
    if (clipsY && scrollableMatcher.test(overflowY)) {
      adjustedTop -= calculateSimulatedScrollOffset(
          rect.top, rect.height, clipTop, clipBottom,
          clippingElement.scrollHeight, clippingElement.clientHeight,
          clippingElement.scrollTop);
    }

    // Adjust for horizontal scroll direction.
    if (clipsX && scrollableMatcher.test(overflowX)) {
      adjustedLeft -= calculateSimulatedScrollOffset(
          rect.left, rect.width, clipLeft, clipRight,
          clippingElement.scrollWidth, clippingElement.clientWidth,
          clippingElement.scrollLeft);
    }

    rect = new DOMRect(adjustedLeft, adjustedTop, rect.width, rect.height);
  }

  // Clips the adjusted box with the clipping element.
  const targetClipBox = {
    left: clipsX ? clipLeft : -Infinity,
    right: clipsX ? clipRight : Infinity,
    top: clipsY ? clipTop : -Infinity,
    bottom: clipsY ? clipBottom : Infinity,
  };

  return clipRect(rect, targetClipBox);
}

/**
 * Calculates the bounding rectangle of an element, clipped by all of its
 * containing block ancestors' boundaries.
 *
 * @param element The element to get the clipped bounds for.
 * @param shouldAdjustRectForScroll Whether to simulate scroll adjustments on
 *     scrollable ancestors.
 * @return The clipped bounding rectangle, or null if it is completely
 *     clipped out.
 */
export function getVisibleRectRespectingClips(
    element: Element, shouldAdjustRectForScroll: boolean): DOMRectReadOnly|
    null {
  let visibleRect: DOMRectReadOnly|null = element.getBoundingClientRect();

  // Using a two-pointer approach, iterates through all ancestor containing
  // blocks and clip the element's bounding rectangle with each block's
  // boundaries.
  let descendant = element;
  let descendantStyle = window.getComputedStyle(descendant);
  let ancestor = descendant.parentElement;

  while (ancestor && visibleRect) {
    const ancestorStyle = window.getComputedStyle(ancestor);

    if (establishesContainingBlockFor(ancestorStyle, descendantStyle)) {
      visibleRect =
          clipRectWithElement(visibleRect, ancestor, shouldAdjustRectForScroll);
      descendant = ancestor;
      descendantStyle = ancestorStyle;
    }

    ancestor = ancestor.parentElement;
  }

  return visibleRect;
}
