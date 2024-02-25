# Chrome Screen AI Library

## Purpose
Chrome Screen AI library provides two on-device functionalities for Chrome and ChromeOS:
 - **Main Content Extraction:** Intelligently isolates the core content of a web
   page, improving its readability by stripping distracting elements (based on
   the accessibility tree).
 - **OCR:** Analyzes images to extract text.

## Development and Deployment
 - **Source:** Developed within Google's internal source code repository (google3).
 - **Platforms:** Built for ChromeOS, Linux, Mac, and Windows.
 - **Distribution:**
   - **ChromeOS:** Distributed via DLC (Dynamic Link Component).
   - **Linux, Mac, Windows:** Delivered on-demand through the component updater.

## How to Use
 - **Library Availability Check:** Since the library isn't pre-installed with
   Chrome, confirm its presence before enabling Screen AI features by
   using `ScreenAIServiceRouter::GetServiceStateAsync` to query the library's
   download and readiness status.
 - **Interface Creation:** Once the library is confirmed available, create a mojo interface to access the desired functionality:
   - Main Content Extraction: Refer to
     `chrome/renderer/accessibility/ax_tree_distiller.cc` focusing on the `ExtractMainContent` call for an example.
   - OCR: Refer to `components/pdf/renderer/pdf_accessibility_tree.cc`,
     focusing on the `PerformOcrAndReturnAXTreeUpdate` call for an example.

## Bugs Component:
- Public:
  - Chromium > UI > Accessibility > MachineIntelligence
- Google Internal:
  - ChromeOS > Software > Experiences > Accessibility > MachineIntelligence
  - ChromeOS Public Tracker > Experiences > Accessibility > Machine Intelligence
