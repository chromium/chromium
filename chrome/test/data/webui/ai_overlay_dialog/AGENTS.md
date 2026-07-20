# TTC WebUI Testing Constraints (`AGENTS.md`)

Rules in this file prevent brittle test doubles and compiler breaks when writing Mocha (`*test.ts`) tests.

## 1. Public-API Testing Only (No `any` Casts)
* **Never access private members via `any` (`(session as any).socket`) or invoke private methods (`session._sendSetup()`).** 
* When a production class manages network, WebSocket, or audio connections (`WebSocket`, `AudioContext`, `SpeechRecognition`), use its dependency-injection factory hook (`webSocketFactory?: (url: string) => WebSocket`) or intercept via public callbacks (`session.connect()`, `fakeWs.onopen()`).

## 2. Test Doubles (`TestBrowserProxy`)
* **Never declare ad-hoc object stubs for Mojo/WebUI remotes.** Always subclass or instantiate `TestBrowserProxy` (`new TestBrowserProxy(['executeTool', ...])`) to ensure type-safe method tracking (`proxy.whenCalled(...)`).
