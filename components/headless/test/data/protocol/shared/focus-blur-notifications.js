// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const {page, dp} =
      await testRunner.startBlank('Tests focus/blur notifications.');

  await dp.Runtime.enable();

  dp.Runtime.onConsoleAPICalled(data => {
    const text = data.params.args[0].value;
    testRunner.log(text);
    if (text === 'quit') {
      testRunner.completeTest();
    }
  });

  // Chrome optimizes away onfocus/onblur notifications if the target page
  // is not active, so activate focus emulation explicitly.
  await dp.Emulation.setFocusEmulationEnabled({enabled: true});

  await page.loadHTML(`
    <!DOCTYPE html>
    <html lang="en">
      <script>
        let counter = 0;

        function onLoad() {
          setTimeout(swapFocus, 0);
        }

        function swapFocus() {
          const id = 'button' + (1 + (counter++ & 1));
          document.getElementById(id).focus();

          if (counter === 4) {
            console.log('quit');
          } else {
            setTimeout(swapFocus, 0);
          }
        }

        function onFocus(id) {
          console.log('onfocus ' + id);
        }
        function onBlur(id) {
          console.log('onblur  ' + id);
        }
      </script>

      <body onload="onLoad()">
        <div>
          <button id="button1" class="button"
            onfocus="onFocus('button1')" onblur="onBlur('button1')">
          Button1
          </button>
        </div>
        <div>
          <button id="button2" class="button"
            onfocus="onFocus('button2')" onblur="onBlur('button2')">
          Button2
          </button>
        </div>
      </body>
    </html>
  `);
});
