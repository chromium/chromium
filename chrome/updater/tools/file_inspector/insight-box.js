// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { ByteSpan, hover } from "./parser.js";

customElements.define("insight-box", class extends HTMLElement {
  /* Class properties:
       title: the title of the finding
       data: an object representing the data within this insight.
             data is either a ByteSpan or an object containing
             additional data.
       hoverSet{index: element}: a mechanism to register for
             hovering callbacks.
  */

  connectedCallback() {
    const shadow = this.attachShadow({mode: "open"})
    const style = document.createElement("style");
    shadow.appendChild(element("style", {}, [text(`
      div {
        margin-left: 1em;
      }
    `)]));
    let box =
      element("div", {}, [
        element("span", {}, [
          this.data instanceof ByteSpan
            ? text(this.title + ": " + this.data.value)
            : text(this.title)])]);
    if (!(this.data instanceof ByteSpan)) {
      for (let t in this.data) {
        let c = element("insight-box");
        c.title = t;
        c.data = this.data[t];
        c.hoverSet = this.hoverSet;
        box.appendChild(c);
      }
    } else {
      let index = this.data.start;
      let hoverSet = this.hoverSet;
      hoverSet[index] = box;
      box.addEventListener("mouseover", () => hover(index, true, hoverSet));
      box.addEventListener("mouseout", () => hover(index, false, hoverSet));
    }
    shadow.appendChild(box);
  }
});

// Helper functions
function element(type, attributes = {}, children = []) {
  let e = document.createElement(type);
  for (let i in attributes) e.setAttribute(i, attributes[i]);
  for (let i of children) e.appendChild(i);
  return e;
}

function text(t) {
  return document.createTextNode(t);
}
