# VRP Flags

This component provides a target for controlled read and controlled write
that can be sent to a renderer when the `--vrp-flags` argument is provided
to Chrome. See the [VRP FAQ](docs/security/vrp-faq.md) for details of how
to submit reports making use of the flags.

`//components/vrp_flags` is a base-like component that can be consumed
by other components like `//components/viz` and `//content/browser`.

## Usage

```html
// --vrp-flags --no-first-run --enable-blink-features=MojoJS

<div id="output"></div>

<script type="module">
  import { VrpFlagsFactory, VrpFlagsRemote } from 'gen/components/vrp_flags/vrp_flags.mojom.m.js';
  function log(msg) {
    console.log(msg);
    const output = document.getElementById('output');
    output.innerHTML += msg + '<br>';
  }
  function createProcessUI(name, remote, writeLocations, writeValue) {
    const container = document.createElement('div');
    container.className = 'process-section';
    const title = document.createElement('h2');
    title.textContent = name;
    container.appendChild(title);
    const info = document.createElement('p');
    info.innerHTML = `Write Locations: ${writeLocations.join(', ')} <br> Write Value: ${writeValue}`;
    container.appendChild(info);
    const firstLocation = writeLocations.length > 0 ? writeLocations[0] : 0n;
    const btnWrite = document.createElement('button');
    btnWrite.textContent = `Attempt Write at ${firstLocation}`;
    btnWrite.onclick = async () => {
      try {
        log(`[${name}] Sending WriteAttempted for location ${firstLocation}...`);
        const result = await remote.writeAttempted(firstLocation);
        log(`[${name}] WriteAttempted Result: ${result.success}`);
      } catch (e) {
        log(`[${name}] Write Error: ${e}`);
      }
    };
    container.appendChild(btnWrite);
    const btnRead = document.createElement('button');
    btnRead.textContent = `Attempt Read (Dummy Token)`;
    btnRead.onclick = async () => {
      try {
        // Get prefix first, then try to read with a dummy token
        log(`[${name}] Calling GetReadPrefix...`);
        const prefixResult = await remote.getReadPrefix();
        log(`[${name}] Prefix received: high=${prefixResult.prefix.high}, low=${prefixResult.prefix.low}`);
        const dummyToken = { high: 123456789n, low: 987654321n };
        log(`[${name}] Sending ReadAttempted with dummy token...`);
        const readResult = await remote.readAttempted(dummyToken);
        log(`[${name}] ReadAttempted Result: ${readResult.success}`);
      } catch (e) {
        log(`[${name}] Read Error: ${e}`);
      }
    };
    container.appendChild(btnRead);
    document.getElementById('controls').appendChild(container);
  }
  async function run() {
    try {
      log("Binding VrpFlagsFactory...");
      const factory = VrpFlagsFactory.getRemote();
      log("Successfully bound VrpFlagsFactory.<br>");
      // 1. Browser
      try {
        const browserVrp = new VrpFlagsRemote();
        factory.bindBrowserVrpFlags(browserVrp.$.bindNewPipeAndPassReceiver());
        const browserRes = await browserVrp.getWriteLocations();
        createProcessUI('Browser', browserVrp, browserRes.locations, browserRes.value);
      } catch (e) {
        log('<strong>Browser VRP Error:</strong> ' + e);
      }
      // 2. Network
      try {
        const networkVrp = new VrpFlagsRemote();
        factory.bindNetworkVrpFlags(networkVrp.$.bindNewPipeAndPassReceiver());
        const networkRes = await networkVrp.getWriteLocations();
        createProcessUI('Network', networkVrp, networkRes.locations, networkRes.value);
      } catch (e) {
        log('<strong>Network VRP Error:</strong> ' + e);
      }
      // 3. GPU
      try {
        const gpuVrp = new VrpFlagsRemote();
        factory.bindGpuVrpFlags(gpuVrp.$.bindNewPipeAndPassReceiver());
        const gpuRes = await gpuVrp.getWriteLocations();
        createProcessUI('GPU', gpuVrp, gpuRes.locations, gpuRes.value);
      } catch (e) {
        log('<strong>GPU VRP Error:</strong> ' + e);
      }
    } catch (e) {
      log('<strong style="color:red">Error:</strong> ' + e);
    }
  }
  run();
</script>
```
